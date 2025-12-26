# Stratum

![License](https://img.shields.io/badge/license-MIT-blue.svg)
![Standard](https://img.shields.io/badge/c%2B%2B-20-blue.svg)
![Build](https://img.shields.io/badge/build-passing-brightgreen.svg)

**A compile-time cache hierarchy simulator for hardware architects.**

Stratum is a zero-overhead, template-based cache simulator designed to help **computer architects rapidly explore and validate cache configurations** before committing to RTL implementation. Powered by Racket meta-programming and C++20 compile-time polymorphism, it enables fast iteration on cache hierarchies with deterministic, cycle-accurate simulation.

## Why Stratum?

Hardware architects face critical decisions when designing cache hierarchies:

- **What cache sizes optimize for my workload?** (32KB L1 vs 64KB?)
- **How many ways should I use?** (4-way vs 8-way associativity?)
- **What's the performance impact of different replacement policies?** (LRU vs FIFO vs Random?)
- **How does latency at each level affect overall performance?**

Stratum answers these questions **before tape-out** by:

1. **Rapid Configuration**: Define cache hierarchies in Racket, generate optimized C++ simulators automatically
2. **Zero Runtime Overhead**: Template metaprogramming eliminates virtual dispatch - hierarchy traversal is resolved at compile-time
3. **Deterministic Results**: Trace-driven simulation ensures reproducible performance analysis
4. **Easy Comparison**: Generate multiple configurations (case_001, case_002, ...) and compare results side-by-side

## Quick Start

### Prerequisites

- **C++20 compiler** (GCC 11+, Clang 12+)
- **CMake 3.10+**
- **Racket 8.0+** (optional, for generating custom configurations)

### Build and Run

```bash
# Clone and build
git clone https://github.com/TheCloudlet/Stratum.git
cd Stratum
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build

# Run the default simulation
./build/bin/stratum

# Run generated experiment configurations
./build/bin/case_001
./build/bin/case_002
```

### Expected Output

```
=========================================================
Running Simulation: Sequential (/path/to/sequential.txt)
=========================================================

=== Simulation Results (Aggregated) ===
Level                 Hits     Misses    Avg Latency (cyc)
L1                    4375        625                    4
L2                       0        625                    0
L3                       0        625                    0
MainMemory             625          0                  232
```

## Architecture Exploration Workflow

### 1. Define Cache Configurations (Racket)

Edit `scripts/config.rkt` to define your cache experiments:

```racket
;; Case 001: Standard 3-Level Hierarchy
(define case-001
  (experiment "case_001"
    (list (cache-config "L1" 64 8 4 "L2")      ; 64 sets, 8-way, 4 cyc latency
          (cache-config "L2" 512 8 10 "L3")    ; 512 sets, 8-way, 10 cyc
          (cache-config "L3" 8192 16 20 "MainMemory")))) ; 8192 sets, 16-way, 20 cyc

;; Case 002: Aggressive L1 (larger, lower latency)
(define case-002
  (experiment "case_002"
    (list (cache-config "L1" 128 8 3 "L2")     ; 2x larger L1
          (cache-config "L2" 512 8 10 "MainMemory"))))
```

### 2. Generate Simulators

```bash
racket scripts/config.rkt
# Generates: build/generated/case_001.cpp, case_002.cpp, CMakeLists.txt
```

Or let CMake auto-generate during build (Racket is auto-detected):

```bash
cmake -B build && cmake --build build
# Automatically runs config.rkt and builds all experiments
```

### 3. Run and Compare

```bash
./build/bin/case_001 > results_001.txt
./build/bin/case_002 > results_002.txt
diff results_001.txt results_002.txt
```

## Manual Configuration (C++)

For one-off experiments, directly edit `src/main.cpp`:

```cpp
using namespace stratum;

// Bottom-up hierarchy definition
using MemType = MainMemory<"MainMemory">;
using L3Type = Cache<"L3", MemType, 8192, 16, 64, LRUPolicy, 20>;
using L2Type = Cache<"L2", L3Type, 512, 8, 64, LRUPolicy, 10>;
using L1Type = Cache<"L1", L2Type, 64, 8, 64, LRUPolicy, 4>;
//                    ^Name  ^Next  ^Sets ^Ways ^BlockSize ^Policy ^Latency

// Run simulation
RunTraceSimulation<L1Type>("MyTrace", "trace.txt", {"L1", "L2", "L3", "MainMemory"});
```

## Trace Files

Stratum includes 6 representative workloads in `test/data/`:

| Trace            | Pattern                         | Expected Behavior                    |
| ---------------- | ------------------------------- | ------------------------------------ |
| `sequential.txt` | Streaming (stride = block size) | 0% hit rate (compulsory misses)      |
| `temporal.txt`   | Hot working set (5 blocks)      | ~100% hit rate (temporal locality)   |
| `spatial.txt`    | Sequential words in block       | 87.5% hit rate (spatial locality)    |
| `random.txt`     | Uniform random                  | ~0% hit rate (no locality)           |
| `largeloop.txt`  | 64KB loop (exceeds L1)          | L2 hits, L1 misses                   |
| `gaussian.txt`   | Normal distribution             | Partial hits (depends on cache size) |

Generate custom traces:

```bash
python scripts/gen_test_data.py
```

## Project Structure

```
stratum/
├── include/stratum/
│   ├── cache_sim.hpp       # Core cache template & statistics
│   ├── policies.hpp        # Replacement policies (LRU, FIFO, Random)
│   ├── simulation.hpp      # Simulation runner & trace parser
│   └── trace_parser.hpp    # Trace file I/O
├── src/
│   └── main.cpp            # Default configuration
├── scripts/
│   ├── config.rkt          # Racket meta-generator
│   └── gen_test_data.py    # Trace file generator
├── test/data/              # Benchmark traces
├── build/
│   ├── bin/                # Compiled executables
│   └── generated/          # Auto-generated experiments
└── CMakeLists.txt
```

## Limitations

### Current Scope: Hierarchical Topologies Only

Stratum currently supports **strictly hierarchical (tree-like) cache organizations**:

- ✅ L1 → L2 → L3 → Memory (inclusive/exclusive hierarchies)
- ✅ Private L1/L2 per core → Shared L3
- ❌ **Mesh/network topologies** (e.g., tiled architectures with NoC)
- ❌ **Non-inclusive/NUCA caches** with complex routing

**Why?** The zero-abstraction template approach relies on compile-time type composition (`Cache<NextLevel>`), which naturally models hierarchical parent-child relationships. Mesh topologies require runtime routing decisions that would break the zero-overhead guarantee.

**Future Work**: Mesh support is possible via hybrid approaches (templates for local hierarchy + runtime routing), but would sacrifice some compile-time optimizations.

## Use Cases

- **Pre-Silicon Architecture Exploration**: Validate cache configurations before RTL design
- **Research**: Study replacement policy effectiveness, cache sensitivity analysis
- **Education**: Teach cache hierarchy concepts with deterministic, observable behavior
- **Performance Modeling**: Estimate memory subsystem performance for new workloads

## Contributing

Contributions welcome! Areas of interest:

- Additional replacement policies (PLRU, ARC, etc.)
- Prefetcher models
- Multi-core simulation (private L1/L2, shared L3)
- Power/energy modeling

Please ensure C++20 compliance and include unit tests.

## License

MIT License - see LICENSE file for details.

## Citation

If you use Stratum in research, please cite:

```bibtex
@software{stratum2025,
  title={Stratum: A Zero-Overhead Cache Hierarchy Simulator},
  author={TheCloudlet},
  year={2025},
  url={https://github.com/TheCloudlet/Stratum}
}
```
