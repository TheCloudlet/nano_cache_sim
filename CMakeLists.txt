cmake_minimum_required(VERSION 3.10)
project(Stratum VERSION 1.0)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED True)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

# Include directories
include_directories(include)

# Main Executable
add_executable(stratum src/main.cpp)
target_compile_definitions(stratum PRIVATE STRATUM_ROOT="${CMAKE_SOURCE_DIR}")

# Testing
enable_testing()
add_executable(unit_tests test/unit/test_main.cpp)
target_compile_definitions(unit_tests PRIVATE STRATUM_ROOT="${CMAKE_SOURCE_DIR}")
add_test(NAME UnitTests COMMAND unit_tests)

# Generated Experiments
find_program(RACKET_EXECUTABLE NAMES racket PATHS ${CMAKE_CURRENT_SOURCE_DIR})
if(RACKET_EXECUTABLE)
  message(STATUS "Found Racket: ${RACKET_EXECUTABLE}")
  execute_process(
    COMMAND ${RACKET_EXECUTABLE} scripts/config.rkt "${CMAKE_BINARY_DIR}/generated"
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
    RESULT_VARIABLE RACKET_RESULT
  )
  if(NOT RACKET_RESULT EQUAL 0)
    message(WARNING "Racket script failed to run.")
  endif()
else()
  message(WARNING "Racket not found. Racket is required to generate experiment code.")
endif()

if(EXISTS "${CMAKE_BINARY_DIR}/generated/CMakeLists.txt")
  add_subdirectory(${CMAKE_BINARY_DIR}/generated)
endif()
