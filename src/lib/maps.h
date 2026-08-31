#pragma once

#include "../lang/native.h"

#include <span>

std::span<const NativeDef>
mapNativeDefinitions() noexcept;

std::span<const NativePropertyDef>
mapNativeProperties() noexcept;

std::span<const NativeMethodDef>
mapNativeMethods() noexcept;
