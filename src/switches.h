#pragma once
#include <limits>

// #define DEBUG_TRACE_EXECUTION
// #define DEBUG_PRINT_CODE
// #define DEBUG_STRESS_GC
// #define DEBUG_LOG_GC
// #define DEBUG_VALUE_TABLE

// #define RELEASE_UNCHECKED_STACK
// #define RELEASE_UNCHECKED_CAST
// #define RELEASE_UNCHECKED_CASES

#define BETTER_HASH_TABLE
#define NAN_BOXING

constexpr int UINT8_COUNT = UINT8_MAX+1;
constexpr int FRAMES_MAX = 64;
constexpr int STACK_MAX = FRAMES_MAX * UINT8_COUNT;
constexpr float MAX_LOAD_FACTOR_F = 0.75;
constexpr int MAX_LOAD_FACTOR_I = static_cast<int>(MAX_LOAD_FACTOR_F*100);
constexpr int HEAP_GROW_FACTOR = 2;
constexpr size_t GC_MIN_THRESH = 1024 * 1024;
constexpr double ZERO_EPSILON = 1e-11;
constexpr double APPROX_EPSILON = 1e-11;
constexpr double MAX_INDEX = 9007199254740991.0;
