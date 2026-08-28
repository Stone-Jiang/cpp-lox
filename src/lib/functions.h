#pragma once

#include "../lang/native.h"

std::span<const NativePropertyDef> functionNativeProperties() noexcept;
std::span<const NativeMethodDef> functionNativeMethods() noexcept;
