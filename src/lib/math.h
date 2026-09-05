#pragma once

#include <complex>
#include "../lang/native.h"

double disregardZero(double value);
std::complex<double> disregardZero(std::complex<double> value);
bool isFinite(const std::complex<double>& value);
NativeResult failure(ErrorKind kind, std::string_view message, Value payload = Value());
NativeResult typeError(std::string_view function, std::string_view expected);
NativeResult valueError(std::string_view function, std::string_view message);
NativeResult domainError(std::string_view function, std::string_view message);
NativeResult rangeError(std::string_view function, std::string_view message);
bool realValue(std::string_view function, const Value& value, double& result, NativeResult& error);
bool numericValue(std::string_view function, const Value& value, std::complex<double>& result, NativeResult& error);
NativeResult realResult(std::string_view function, double value);
NativeResult complexResult(std::string_view function, VM& vm, const std::complex<double>& value);

std::span<const NativeDef> mathNativeDefinitions() noexcept;
