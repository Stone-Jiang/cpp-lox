#pragma once

#include "../lang/native.h"

std::span<const NativePropertyDef> stringNativeProperties() noexcept;
std::span<const NativeMethodDef> stringNativeMethods() noexcept;

// Converts string data into an immutable, interned Lox string while
// translating allocation failures into language-level native errors.
NativeResult makeStringResult(VM& vm, std::string_view value);
