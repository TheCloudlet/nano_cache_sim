// Copyright 2025-2026 Yi-Ping Pan (Cloudlet)

#ifndef TRACE_PARSER_HPP
#define TRACE_PARSER_HPP

#include <cstdint>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>

#include "stratum/print.hpp"

namespace stratum {

struct TraceOp {
  char type;  // 'L' or 'S'
  uint64_t addr;
};

/**
 * ParseTraceFile: Reads memory access patterns from a text file.
 * Expects format: "L 0x1234" or "S 0x5678" per line.
 */
inline std::vector<TraceOp> ParseTraceFile(const std::string& filepath) {
  std::vector<TraceOp> ops;
  std::ifstream infile(filepath);

  if (!infile.is_open()) {
    PrintErr("Error: Could not open trace file: {}\n", filepath);
    return ops;
  }

  std::string line;
  while (std::getline(infile, line)) {
    if (line.empty() || line[0] == '#') continue;

    std::stringstream ss(line);
    char type;
    std::string addr_str;

    if (ss >> type >> addr_str) {
      uint64_t addr = 0;
      try {
        addr = std::stoull(addr_str, nullptr, 16);
      } catch (...) {
        PrintErr("Warning: Skipping invalid line: {}\n", line);
        continue;
      }
      ops.push_back({type, addr});
    }
  }

  return ops;
}

}  // namespace stratum

#endif  // TRACE_PARSER_HPP
