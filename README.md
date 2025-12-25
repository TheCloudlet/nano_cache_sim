# Stratum

![License](https://img.shields.io/badge/license-MIT-blue.svg)
![Standard](https://img.shields.io/badge/c%2B%2B-20-blue.svg)
![Build](https://img.shields.io/badge/build-passing-brightgreen.svg)

A compile-time, zero-overhead cache hierarchy simulator generator. Powered by Racket (Lisp) meta-generation and C++20 Policy-based Design.

**Stratum** is a high-performance, template-based cache simulator written in C++20. It leverages compile-time polymorphism to define complex cache hierarchies with zero runtime overhead for hierarchy traversal.

Designed for modularity and extensibility, Stratum allows researchers and developers to model arbitrary cache levels (L1, L2, L3, etc.), associativity, block sizes, and replacement policies using clean, declarative C++ templates.

## Key Features

- **Zero-Overhead Hierarchy**: Hierarchy connections are resolved at compile-time using recursive template instantiation.
- **Flexible Configuration**: Define cache geometry (sets, ways, line size) and policies as template arguments.
- **Pluggable Policies**: Easily implement custom replacement policies (LRU, FIFO, Random, etc.).
- **Detailed Statistics**: Automatic aggregation of hits, misses, latency, and cycle counts per level.
- **Trace-Driven**: Supports standard load/store trace formats for deterministic simulation.

## Build & Run

### Prerequisites

- C++20 compliant compiler (GCC 10+, Clang 12+, MSVC 2019+)
- CMake 3.10+

### Building

```bash
mkdir build && cd build
cmake ..
make
```

### Running Simulations

Stratum comes with a set of trace files for validation.

```bash
./stratum
```

To run unit tests:

```bash
./unit_tests
```

## Configuration

The cache hierarchy is defined in `src/main.cpp`. Stratum uses a "bottom-up" definition style where you define the main memory first, then build layers on top of it.

```cpp
using namespace stratum;

// 1. Define Memory (Bottom of hierarchy)
using MemType = MainMemory<"MainMemory">;

// 2. Define Caches (Recursive composition)
// Cache<Name, NextLevel, Sets, Ways, BlockSize, Policy, HitLatency>
using L3Type = Cache<"L3", MemType, 8192, 16, 64, LRUPolicy, 20>;
using L2Type = Cache<"L2", L3Type, 512, 8, 64, LRUPolicy, 10>;
using L1Type = Cache<"L1", L2Type, 64, 8, 64, LRUPolicy, 4>;

// 3. Run Simulation (TraceFile, TopLevelCache)
RunTraceSimulation<L1Type>("MyTrace", "path/to/trace.txt", {"L1", "L2", "L3", "MainMemory"});
```

## Project Structure

```
stratum/
├── include/
│   └── stratum/
│       ├── cache_sim.hpp    # Core Cache Template & Statistics
│       ├── policies.hpp     # Replacement Policies (LRU, FIFO, Random)
│       ├── simulation.hpp   # Simulation Loop & Trace Runner
│       └── trace_parser.hpp # Trace File Parsing
├── src/
│   └── main.cpp             # Configuration & Entry Point
├── test/
│   ├── data/                # Trace files
│   └── unit/                # Unit tests
└── CMakeLists.txt           # Build configuration
```

## Contributing

Contributions are welcome! Please ensure any new features utilize C++20 concepts where appropriate and include unit tests.

## License

This project is licensed under the MIT License - see the LICENSE file for details.
