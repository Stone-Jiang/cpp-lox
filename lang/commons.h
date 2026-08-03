#pragma once

#include <iostream>
#include <string>
#include <string_view>
#include <format>
#include <vector>
#include <array>
#include <unordered_map>
#include <optional>
#include <variant>
#include <memory>
#include <stdexcept>
#include <cctype>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <time.h>

using std::string;

using u8 = uint8_t;
using u16 = uint16_t;
using u32 = uint32_t;
using u64 = uint64_t;

const int UINT8_COUNT = UINT8_MAX+1;
const int FRAMES_MAX = 64;
const int STACK_MAX = FRAMES_MAX * UINT8_COUNT;

// #define DEBUG_TRACE_EXECUTION
// #define DEBUG_PRINT_CODE
// #define DEBUG_VALUE_INFO
// #define DEBUG_STRESS_GC
// #define DEBUG_LOG_GC
// #define DEBUG_VALUE_TYPENAME

// #define NAN_BOXING

const float MAX_LOAD_FACTOR = 0.75;