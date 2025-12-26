#ifndef CACHE_HPP
#define CACHE_HPP

#include <fmt/core.h>

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <iostream>
#include <map>
#include <memory>
#include <string>
#include <vector>

#include "stratum/policies.hpp"

namespace stratum {

struct AccessResult {
  std::string hit_level;
  size_t total_cycles;
};

struct CacheStats {
  size_t hits = 0;
  size_t misses = 0;
  size_t total_latency = 0;
};

inline void PrintSimulationStats(const std::vector<AccessResult>& history,
                                 const std::vector<std::string>& hierarchy) {
  std::map<std::string, CacheStats> stats_db;

  for (const auto& res : history) {
    bool hit_found = false;
    for (const auto& level_name : hierarchy) {
      if (level_name == res.hit_level) {
        stats_db[level_name].hits++;
        stats_db[level_name].total_latency += res.total_cycles;
        hit_found = true;
        break;
      } else {
        stats_db[level_name].misses++;
      }
    }
    if (!hit_found) {
      fmt::print(stderr, "Error: Hit level {} not in hierarchy def!\n",
                 res.hit_level);
    }
  }

  fmt::print("\n=== Simulation Results (Aggregated) ===\n");
  fmt::print("{:<15} {:>10} {:>10} {:>20}\n", "Level", "Hits", "Misses",
             "Avg Latency (cyc)");

  for (const auto& level_name : hierarchy) {
    const auto& s = stats_db[level_name];
    double avg_lat = 0.0;
    if (s.hits > 0) avg_lat = (double)s.total_latency / s.hits;

    fmt::print("{:<15} {:>10} {:>10} {:>20.0f}\n", level_name, s.hits, s.misses,
               avg_lat);
  }
}

inline void PrintAccessLog(const std::vector<AccessResult>& history,
                           const std::vector<uint64_t>& trace_addrs) {
  fmt::print("\n=== Detailed History ===\n");
  for (size_t i = 0; i < history.size(); ++i) {
    fmt::print("Access[{:>4}] Addr=0x{:08x} Hit={:<15} Cyc={:>6}\n", i,
               trace_addrs[i], history[i].hit_level, history[i].total_cycles);
  }
}

enum class AccessType { kLoad, kStore };

// C++20 Fixed String for Non-Type Template Parameters
template <size_t N>
struct FixedString {
  char value[N]{};
  constexpr FixedString(const char (&str)[N]) { std::copy_n(str, N, value); }
};

// Main Memory: The bottom of the hierarchy
template <FixedString Name = "MainMemory">
class MainMemory {
  size_t latency_;

 public:
  MainMemory(size_t lat = 100) : latency_(lat) {}

  AccessResult Load(uint64_t addr) {
    return {Name.value, latency_};  // Always hits
  }

  AccessResult Store(uint64_t addr) { return {Name.value, latency_}; }
};

// Cache Template
// Usage: Cache<"L1", NextLayer, ...>
template <FixedString Name,
          typename NextLayer,  // potentially another Cache<...> or MainMemory
          size_t Sets, size_t Ways, size_t BlockSize,
          typename ReplacePolicy = LRUPolicy,
          size_t HitLatency = 1  // Default hit latency
          >
class Cache {
  struct Line {
    bool valid = false;
    bool dirty = false;
    uint64_t tag = 0;
  };

  std::unique_ptr<NextLayer> next_;  // OWNS the next layer

  // Cache State
  std::vector<std::vector<Line>> sets_;
  ReplacePolicy policy_;

  // Stats
  size_t hits_ = 0;
  size_t misses_ = 0;
  size_t evictions_ = 0;

 public:
  // Variadic Constructor: Recursively creates the next layer
  template <typename... Args>
  Cache(Args&&... args)
      : next_(std::make_unique<NextLayer>(std::forward<Args>(args)...)),
        sets_(Sets, std::vector<Line>(Ways)),
        policy_(Sets, Ways) {}

  AccessResult Load(uint64_t addr) {
    uint64_t set_idx = (addr / BlockSize) % Sets;
    uint64_t tag = addr / (BlockSize * Sets);

    // 1. Tag Lookup
    for (size_t way_idx = 0; way_idx < Ways; ++way_idx) {
      if (sets_[set_idx][way_idx].valid && sets_[set_idx][way_idx].tag == tag) {
        // HIT
        StatsHit();
        policy_.OnHit(set_idx, way_idx);
        return {Name.value, HitLatency};
      }
    }

    // 2. MISS - Fetch from next level
    StatsMiss();
    AccessResult res = next_->Load(addr);

    // 3. Accumulate Latency
    res.total_cycles += HitLatency;

    // 4. Update Cache (fill)
    Fill(set_idx, tag);

    return res;
  }

  AccessResult Store(uint64_t addr) {
    uint64_t set_idx = (addr / BlockSize) % Sets;
    uint64_t tag = addr / (BlockSize * Sets);

    // 1. Tag Lookup
    for (size_t way_idx = 0; way_idx < Ways; ++way_idx) {
      if (sets_[set_idx][way_idx].valid && sets_[set_idx][way_idx].tag == tag) {
        // HIT
        StatsHit();
        sets_[set_idx][way_idx].dirty = true;
        policy_.OnHit(set_idx, way_idx);
        return {Name.value, HitLatency};
      }
    }

    // 2. Write Miss -> Write Allocate
    StatsMiss();
    AccessResult res = next_->Load(addr);
    res.total_cycles += HitLatency;

    // 3. Fill and mark dirty
    Fill(set_idx, tag);

    // Mark the newly filled line as dirty
    for (size_t way_idx = 0; way_idx < Ways; ++way_idx) {
        if (sets_[set_idx][way_idx].valid && sets_[set_idx][way_idx].tag == tag) {
            sets_[set_idx][way_idx].dirty = true;
            break;
        }
    }

    return res;
  }


  // Helper to print stats
  void PrintStats() const {
    fmt::print("Cache {}: Hits={}, Misses={}, Evictions={}\n", Name.value,
               hits_, misses_, evictions_);
  }

  void PrintAllStats() const {
    PrintStats();
    // next_->PrintAllStats(); // Requires generic interface or SFINAE
  }

  NextLayer* GetNext() const { return next_.get(); }

 private:
  void Fill(size_t set_idx, uint64_t tag) {
    size_t victim_way_idx = Ways;
    for (size_t way_idx = 0; way_idx < Ways; ++way_idx) {
      if (!sets_[set_idx][way_idx].valid) {
        victim_way_idx = way_idx;
        break;
      }
    }

    if (victim_way_idx == Ways) {
      victim_way_idx = policy_.GetVictim(set_idx);
      Line& victim = sets_[set_idx][victim_way_idx];
      if (victim.valid && victim.dirty) {
        uint64_t evict_addr = (victim.tag * Sets + set_idx) * BlockSize;
        next_->Store(evict_addr);
        evictions_++;
      }
    }

    sets_[set_idx][victim_way_idx].valid = true;
    sets_[set_idx][victim_way_idx].tag = tag;
    sets_[set_idx][victim_way_idx].dirty = false;
    policy_.OnFill(set_idx, victim_way_idx);
  }

  void StatsHit() { hits_++; }
  void StatsMiss() { misses_++; }
};

}  // namespace stratum

#endif  // CACHE_HPP
