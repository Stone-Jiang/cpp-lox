#pragma once

#include "../lang/native.h"

#include <span>

std::span<const NativeDef>
fileNativeDefinitions() noexcept;

std::span<const NativePropertyDef>
fileNativeProperties() noexcept;

std::span<const NativeMethodDef>
fileNativeMethods() noexcept;
