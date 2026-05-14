// Copyright 2025-2026 Yi-Ping Pan (Cloudlet)

#ifndef STRATUM_PRINT_HPP
#define STRATUM_PRINT_HPP

#include <format>
#include <iostream>
#include <string_view>

namespace stratum {

/**
 * Print: Centralized wrapper for std::format + std::cout.
 * Provides a clean, type-safe API similar to Abseil or C++23's std::print.
 */
template <typename... Args>
void Print(std::string_view fmt, Args&&... args) {
  std::cout << std::vformat(fmt, std::make_format_args(args...));
}

/**
 * PrintErr: Centralized wrapper for std::format + std::cerr.
 */
template <typename... Args>
void PrintErr(std::string_view fmt, Args&&... args) {
  std::cerr << std::vformat(fmt, std::make_format_args(args...));
}

}  // namespace stratum

#endif  // STRATUM_PRINT_HPP
