#pragma once

#include "error.h"
#include "value.h"

#include <span>
#include <string>
#include <string_view>

class VM;

struct NativeResult
{
    bool ok = true;
    Value value;
    NativeError error;

    static NativeResult success(Value value)
    {
        NativeResult result;
        result.value = value;
        return result;
    }

    static NativeResult failure(ErrorKind kind, std::string message, Value payload = Value())
    {
        NativeResult result;
        result.ok = false;
        result.error = NativeError{kind, std::move(message), payload};
        return result;
    }
};

using NativeFn = NativeResult (*)(VM& vm, int argCount, Value* args);

struct NativeDef
{
    std::string_view name;
    int arity;
    NativeFn function;
};

std::span<const NativeDef> nativeDefinitions() noexcept;
