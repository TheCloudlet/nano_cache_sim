// Copyright 2025-2026 Yi-Ping Pan (Cloudlet)

#ifndef SIMULATION_HPP
#define SIMULATION_HPP

#include <memory>
#include <string>
#include <vector>

#include "stratum/cache_sim.hpp"
#include "stratum/print.hpp"
#include "stratum/trace_parser.hpp"

namespace stratum {

// Runs a trace-driven cache simulation and prints performance statistics.
//
// This function simulates a complete cache hierarchy by:
// 1. Parsing a trace file containing memory access operations
// 2. Executing each operation (Load/Store) through the cache system
// 3. Recording access results (hit level, latency) for each operation
// 4. Aggregating and printing statistics per cache level
//
// Template Parameters:
//   CacheSystem: Top-level cache type (e.g., L1Type). Must provide:
//     - Constructor accepting size_t (memory latency)
//     - Load(uint64_t addr) -> AccessResult
//     - Store(uint64_t addr) -> AccessResult
//
// Parameters:
//   trace_name: Human-readable name for this trace (e.g., "Sequential")
//   filepath: Path to trace file (format: "L 0x1000" or "S 0x2000")
//   hierarchy: Cache level names in order (e.g., {"L1", "L2", "MainMemory"})
//   mem_latency: Main memory access latency in cycles (default: 100)
//
// Example:
//   RunTraceSimulation<L1Type>("Temporal", "traces/temporal.txt",
//                              {"L1", "L2", "MainMemory"}, 200);
//
// Output:
//   - Simulation header with trace name and file path
//   - Aggregated statistics (hits, misses, avg latency per level)
//   - Detailed access log (if trace has ≤20 operations)
template <typename CacheSystem>
void RunTraceSimulation(const std::string& trace_name,
                        const std::string& filepath,
                        const std::vector<std::string>& hierarchy,
                        size_t mem_latency = 100) {
  Print("\n=========================================================\n");
  Print("Running Simulation: {} ({})\n", trace_name, filepath);
  Print("=========================================================\n");

  auto ops = ParseTraceFile(filepath);
  if (ops.empty()) {
    Print("No operations to simulate for {}\n", trace_name);
    return;
  }

  // Initialize cache hierarchy with specified memory latency.
  // The latency parameter propagates down to MainMemory constructor.
  auto cache_system = std::make_unique<CacheSystem>(mem_latency);

  std::vector<AccessResult> history;
  std::vector<uint64_t> trace_addrs;

  // Execute all trace operations and record results.
  for (const auto& op : ops) {
    AccessResult res;
    if (op.type == 'L') {
      res = cache_system->Load(op.addr);
    } else {
      res = cache_system->Store(op.addr);
    }
    history.push_back(res);
    trace_addrs.push_back(op.addr);
  }

  // Print aggregated statistics (hits, misses, latency per level).
  PrintSimulationStats(history, hierarchy);

  // Print detailed access log only for small traces (≤20 ops).
  if (history.size() <= 20) {
    PrintAccessLog(history, trace_addrs);
  } else {
    Print("\n(Detailed history hidden for large trace: {} ops)\n",
          history.size());
  }
}

}  // namespace stratum

#endif  // SIMULATION_HPP
