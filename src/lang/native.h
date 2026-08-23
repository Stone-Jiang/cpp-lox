#pragma once

#include "value.h"

#include <span>
#include <string_view>

class VM;

using NativeFn = Value (*)(VM& vm, int argCount, Value* args);

struct NativeDef
{
    std::string_view name;
    int arity;
    NativeFn function;
};

std::span<const NativeDef> nativeDefinitions() noexcept;
