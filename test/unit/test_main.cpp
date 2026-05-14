// Copyright 2025-2026 Yi-Ping Pan (Cloudlet)

#include "stratum/cache_sim.hpp"
#include "stratum/print.hpp"

// Simple manual test runner for now
// In a real scenario, use GTest or Catch2
int main() {
  stratum::Print("Running Unit Tests...\n");

  using namespace stratum;

  // Tiny Cache for testing evictions
  using Mem = MainMemory<"MainMemory">;
  using TinyCache = Cache<"Tiny", Mem, 1, 2, 64, LRUPolicy, 1>;

  // Construct with latency arg for Mem, which bubbles down
  // Actually, std::make_unique<TinyCache> will call TinyCache()
  // TinyCache() calls Mem().
  // If we want to pass mem latency, we do std::make_unique<TinyCache>(100) or
  // similar. Mem defaults to 100.
  auto cache = std::make_unique<TinyCache>(100);

  // Fill Set 0
  cache->Load(0x0000);  // Way 0
  cache->Load(0x0040);  // Way 1 (Set 0 is full now)

  // Access Way 0 again to make it MRU
  cache->Load(0x0000);

  // Load new block -> Should evict Way 1 (0x0040)
  auto res = cache->Load(0x0080);

  if (res.hit_level == "MainMemory") {
    stratum::Print("[PASS] Eviction Logic\n");
  } else {
    stratum::Print("[FAIL] Eviction Logic. Expected 'MainMemory', got '{}'\n",
                   res.hit_level);
    return 1;
  }

  return 0;
}
