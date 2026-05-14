#lang racket

;; Copyright 2025-2026 Yi-Ping Pan (Cloudlet)

;; =============================================================================
;; Stratum Cache Hierarchy DSL Compiler
;; =============================================================================
;; This is a meta-programming tool that transforms declarative cache hierarchy
;; specifications (S-expressions) into optimized C++ simulation code.
;;
;; Philosophy: Code as Data, Data as Code (Homoiconicity)
;; - Cache configurations are first-class data structures
;; - The compiler is just a series of transformations on lists
;; - Pattern matching replaces manual field extraction
;;
;; Why Racket?
;; - Lisp macros allow us to define a custom syntax for hardware architects
;; - Quasiquoting makes code generation elegant (no string concatenation hell)
;; - Functional style ensures correctness (pure functions, no side effects)
;; =============================================================================

;; =============================================================================
;; 1. DSL Definitions (The "What", not the "How")
;; =============================================================================

;; Cache hierarchy specifications as pure S-expressions.
;; Format: (experiment-name (cache-name sets ways latency policy next-level) ...)
;;
;; Example: (L1 64 8 4 LRUPolicy L2) means:
;;   - Name: L1
;;   - Sets: 64 (total capacity = 64 sets × 8 ways × 64 bytes = 32KB)
;;   - Ways: 8 (8-way set-associative)
;;   - Latency: 4 cycles
;;   - Policy: LRUPolicy (replacement policy: LRU/FIFO/Random)
;;   - Next level: L2 (on miss, fetch from L2)
(define experiments
  `((case_001
     ;; Standard 3-level hierarchy with LRU
     (L1      64   8    4   LRUPolicy   L2)
     (L2      512  8    64  LRUPolicy   L3)
     (L3      8192 16   64  LRUPolicy   MainMemory))

    (case_002
     ;; Aggressive L1, skip L3, still LRU
     (L1      64   8    4   LRUPolicy   L2)
     (L2      512  8    64  LRUPolicy   MainMemory))

    (case_003_fifo
     ;; Compare FIFO vs LRU (same geometry as case_001)
     (L1      64   8    4   FIFOPolicy  L2)
     (L2      512  8    64  FIFOPolicy  L3)
     (L3      8192 16   64  FIFOPolicy  MainMemory))

    (case_004_random
     ;; Baseline: Random replacement (worst-case performance)
     (L1      64   8    4   RandomPolicy L2)
     (L2      512  8    64  RandomPolicy MainMemory))))

;; Add new traces here if you want to simulate real world traces
;; Trace files should be placed in test/data/
;; Format: ("trace_name" "trace_file.txt")
;;
;; Example generation command:
;; valgrind --tool=lackey --trace-mem=yes ./your_program 2>&1 | \
;;   perl -ne 'BEGIN{print "# Type  Addr\n"} \
;;            if(/^ ([LSM])\s+0x([0-9a-fA-F]+)/){ \
;;              $addr=hex($2); \
;;              $aligned=int($addr/64)*64; \
;;              $type=($1 eq "M")?"S":$1; \
;;              printf "%s       0x%X\n",$type,$aligned \
;;            }' > test/data/my_trace.txt
(define traces
  '(("Sequential" "sequential.txt")
    ("Random"     "random.txt")
    ("Temporal"   "temporal.txt")
    ("Spatial"    "spatial.txt")
    ("LargeLoop"  "largeloop.txt")
    ("Gaussian"   "gaussian.txt")))

;; =============================================================================
;; 2. The Compiler (S-Expression → C++ Transformation)
;; =============================================================================

;; Compile a single cache level definition into C++ type alias.
;; Input:  (L1 64 8 4 LRUPolicy L2)
;; Output: "using L1Type = Cache<"L1", L2Type, 64, 8, 64, LRUPolicy, 4>;\n"
(define (compile-cache-def config)
  (match-define (list name sets ways lat policy next) config)

  ;; Type alias mapping: MainMemory → MemType, L2 → L2Type, etc.
  (define next-type
    (if (eq? next 'MainMemory) "MemType" (format "~aType" next)))

  ;; Generate C++ using declaration with policy parameter
  (format "  using ~aType = Cache<\"~a\", ~a, ~a, ~a, 64, ~a, ~a>;\n"
          name name next-type sets ways policy lat))

;; Extract hierarchy names from cache layers for stats tracking.
;; Input:  ((L1 ...) (L2 ...) (L3 ...))
;; Output: '("L1" "L2" "L3" "MainMemory")
(define (extract-hierarchy layers)
  (append (map (lambda (layer) (symbol->string (first layer))) layers)
          '("MainMemory")))

;; Generate trace simulation loop code.
;; Input:  top-level cache type (e.g., "L1")
;; Output: C++ code that runs all traces
(define (generate-trace-loop top-level)
  (apply string-append
         (for/list ([trace traces])
           (match-define (list name file) trace)
           (format "    RunTraceSimulation<~aType>(\"~a\", project_root + \"/test/data/~a\", hierarchy);\n"
                   top-level name file))))

;; Compile a complete experiment into a (filename . content) pair.
;; Input:  (case_001 (L1 ...) (L2 ...) (L3 ...))
;; Output: ("case_001.cpp" . "C++ source code")
(define (compile-experiment exp-def)
  (match-define (list exp-name layers ...) exp-def)

  ;; C++ requires bottom-up definition (L3 before L2 before L1)
  (define reversed-layers (reverse layers))
  (define cache-code (apply string-append (map compile-cache-def reversed-layers)))
  (define top-level (first (first layers)))  ;; Top-level cache (usually L1)
  (define hierarchy (extract-hierarchy layers))

  ;; Return (filename . content) pair
  (cons (format "~a.cpp" exp-name)
        (generate-cpp-template cache-code top-level hierarchy)))

;; Generate complete C++ source file from template.
(define (generate-cpp-template cache-defs top-level hierarchy)
  (format #<<EOF
#include <string>
#include <vector>

#include "stratum/cache_sim.hpp"
#include "stratum/simulation.hpp"

using namespace stratum;

// Generated by Racket DSL Compiler
int main() {
  // Define cache hierarchy (bottom-up: Memory -> L3 -> L2 -> L1)
  using MemType = MainMemory<"MainMemory">;
~a
  // Define hierarchy for statistics aggregation
  std::vector<std::string> hierarchy = ~a;

  // Define test data location
  const std::string project_root = STRATUM_ROOT;

  // Run all benchmark traces
~a
  return 0;
}
EOF
          cache-defs
          (format-hierarchy hierarchy)
          (generate-trace-loop top-level)))

;; Format hierarchy list as C++ initializer list.
;; Input:  '("L1" "L2" "L3" "MainMemory")
;; Output: "{ \"L1\", \"L2\", \"L3\", \"MainMemory\" }"
(define (format-hierarchy hierarchy)
  (format "{ ~a }"
          (string-join (map (lambda (s) (format "\"~a\"" s)) hierarchy) ", ")))

;; =============================================================================
;; 3. CMakeLists.txt Generation
;; =============================================================================

;; Generate CMakeLists.txt for all experiments.
;; Each experiment becomes an executable linked against fmt and with STRATUM_ROOT.
(define (generate-cmake experiments)
  (define targets
    (for/list ([exp experiments])
      (match-define (list exp-name _ ...) exp)
      (format #<<CMAKE
add_executable(~a ~a.cpp)
target_compile_definitions(~a PRIVATE STRATUM_ROOT="${CMAKE_SOURCE_DIR}")

CMAKE
              exp-name exp-name exp-name)))

  (string-append "# Generated by Racket DSL Compiler\n\n"
                 (apply string-append targets)))

;; =============================================================================
;; 4. Main Execution (Side Effects Isolated Here)
;; =============================================================================

;; Get output directory from command-line or use default
(define output-dir
  (let ([args (current-command-line-arguments)])
    (if (> (vector-length args) 0)
        (vector-ref args 0)
        "build/generated")))

(displayln (format "Output directory: ~a" output-dir))

;; Ensure output directory exists (create recursively if needed)
(unless (directory-exists? output-dir)
  (make-directory* output-dir))

;; Generate all C++ experiment files
(for ([exp experiments])
  (match-define (cons fname content) (compile-experiment exp))
  (displayln (format "Generating ~a..." fname))
  (call-with-output-file (build-path output-dir fname)
    (lambda (out) (display content out))
    #:exists 'replace))

;; Generate CMakeLists.txt
(define cmake-filename (build-path output-dir "CMakeLists.txt"))
(displayln (format "Generating ~a..." cmake-filename))
(call-with-output-file cmake-filename
  (lambda (out) (display (generate-cmake experiments) out))
  #:exists 'replace)

(displayln "Done!")
