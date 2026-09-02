#pragma once

#include "../lang/native.h"
#include <span>

std::span<const NativePropertyDef> instanceNativeProperties() noexcept;
std::span<const NativeMethodDef> instanceNativeMethods() noexcept;

std::span<const NativePropertyDef> classNativeProperties() noexcept;
std::span<const NativeMethodDef> classNativeMethods() noexcept;