#pragma once

#include "error.h"
#include "value.h"

#include <span>
#include <string>
#include <string_view>

class VM;
enum class ObjType;

class ObjError;

struct NativeResult
{
    bool ok = true;
    Value value;
    NativeError error;

    static NativeResult success(Value value);
    static NativeResult failure(ErrorKind kind, std::string message, Value payload = Value());
    static NativeResult failure(const ObjError* error);
};

using NativeFn = NativeResult (*)(VM& vm, int argCount, Value* args);

struct NativeDef
{
    std::string_view name;
    int arity;
    NativeFn function;
};

std::span<const NativeDef> nativeDefinitions() noexcept;

using NativeMethodFn = NativeResult (*)(VM& vm, Value receiver, int argCount, Value* args);

struct NativeMethodDef
{
    std::string_view name;
    int arity;
    NativeMethodFn function;
};

using NativePropertyFn = NativeResult (*)(VM& vm, Value receiver);

struct NativePropertyDef
{
    std::string_view name;
    NativePropertyFn getter;
};

struct NativeTypeDef
{
    ObjType type;
    std::span<const NativePropertyDef> properties;
    std::span<const NativeMethodDef> methods;
};

std::span<const NativeTypeDef> nativeTypeDefinitions() noexcept;
const NativePropertyDef* findNativeProperty(ObjType type, std::string_view name) noexcept;
const NativeMethodDef* findNativeMethod(ObjType type, std::string_view name) noexcept;
bool hasNativeType(ObjType type) noexcept;
