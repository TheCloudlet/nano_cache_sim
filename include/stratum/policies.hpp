// Copyright 2025-2026 Yi-Ping Pan (Cloudlet)

#ifndef POLICIES_HPP
#define POLICIES_HPP

#include <cstdint>
#include <random>
#include <vector>

namespace stratum {

class LRUPolicy {
  const size_t num_sets_;
  const size_t num_ways_;

  // Flattened timestamp array for cache locality
  // Layout: [Set0_Way0, Set0_Way1... | Set1_Way0, Set1_Way1...]
  std::vector<uint64_t> timestamps_;

  // Logical clock per set (models hardware counter)
  std::vector<uint64_t> set_counters_;

 public:
  LRUPolicy(size_t sets, size_t ways)
      : num_sets_(sets),
        num_ways_(ways),
        // Pre-allocate all memory: size = sets * ways
        timestamps_(sets * ways, 0),
        set_counters_(sets, 0) {}

  // Update timestamp on hit/fill
  void OnHit(size_t set_idx, size_t way_idx) noexcept {
    // Calculate flat index: set_idx * ways + way_idx
    size_t flat_idx = (set_idx * num_ways_) + way_idx;

    // Increment set counter and tag this way with new timestamp
    // Hardware: Demux selects target register, counter value written
    timestamps_[flat_idx] = ++set_counters_[set_idx];
  }

  void OnFill(size_t set_idx, size_t way_idx) noexcept {
    OnHit(set_idx, way_idx);
  }

  [[nodiscard]] size_t GetVictim(size_t set_idx) const noexcept {
    size_t victim_way = 0;
    uint64_t min_time = std::numeric_limits<uint64_t>::max();

    size_t base_idx = set_idx * num_ways_;

    // Linear scan of all ways in this set
    // Contiguous memory layout enables efficient CPU prefetching
    for (size_t way = 0; way < num_ways_; ++way) {
      if (timestamps_[base_idx + way] < min_time) {
        min_time = timestamps_[base_idx + way];
        victim_way = way;
      }
    }
    return victim_way;
  }
};

// 2. First-In, First-Out (FIFO)
class FIFOPolicy {
  size_t num_sets_;
  size_t num_ways_;
  std::vector<size_t> next_victim_;  // Circular buffer index per set

 public:
  FIFOPolicy(size_t sets, size_t ways) : num_sets_(sets), num_ways_(ways) {
    next_victim_.resize(sets, 0);
  }

  void OnHit(size_t, size_t) {
    // FIFO ignores hits
  }

  void OnFill(size_t set, size_t) {
    // Increment circular buffer
    next_victim_[set] = (next_victim_[set] + 1) % num_ways_;
  }

  size_t GetVictim(size_t set) const { return next_victim_[set]; }
};

// 3. Random Policy
class RandomPolicy {
  size_t num_sets_;
  size_t num_ways_;
  mutable std::mt19937 search_rng_{std::random_device{}()};

 public:
  RandomPolicy(size_t sets, size_t ways) : num_sets_(sets), num_ways_(ways) {}

  void OnHit(size_t, size_t) {}
  void OnFill(size_t, size_t) {}

  size_t GetVictim(size_t) const {
    std::uniform_int_distribution<size_t> dist(0, num_ways_ - 1);
    return dist(search_rng_);
  }
};

}  // namespace stratum

#endif  // POLICIES_HPP
