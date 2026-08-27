#pragma once

#include "../lang/native.h"

#include <span>

std::span<const NativePropertyDef> arrayNativeProperties() noexcept;
std::span<const NativeMethodDef> arrayNativeMethods() noexcept;
