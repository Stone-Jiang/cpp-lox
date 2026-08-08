#pragma once

#include "value.h"

#include <span>
#include <string_view>

using NativeFn = Value (*)(int argCount, Value* args);

struct NativeDef
{
    std::string_view name;
    int arity;
    NativeFn function;
};

std::span<const NativeDef> nativeDefinitions() noexcept;
