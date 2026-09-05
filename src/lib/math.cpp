#include "math.h"

#include "../lang/object.h"
#include "../lang/vm.h"
#include "../utils/special.hpp"

#include <array>
#include <cmath>
#include <complex>

double disregardZero(double value)
{
    return std::abs(value) < ZERO_EPSILON ? 0.0 : value;
}

std::complex<double> disregardZero(std::complex<double> value)
{
    return {disregardZero(value.real()), disregardZero(value.imag())};
}

bool isFinite(const std::complex<double>& value)
{
    return std::isfinite(value.real()) && std::isfinite(value.imag());
}

NativeResult failure(ErrorKind kind, std::string_view message, Value payload)
{
    return NativeResult::failure(kind, std::string(message), payload);
}

NativeResult typeError(std::string_view function, std::string_view expected)
{
    return failure(ErrorKind::TYPE_ERROR,
        std::format("{}() expects {}.", function, expected));
}

NativeResult valueError(std::string_view function, std::string_view message)
{
    return failure(ErrorKind::VALUE_ERROR, std::format("{}() {}.", function, message));
}

NativeResult domainError(std::string_view function, std::string_view message)
{
    return failure(ErrorKind::DOMAIN_ERROR, std::format("{}() {}.", function, message));
}

NativeResult rangeError(std::string_view function, std::string_view message)
{
    return failure(ErrorKind::RANGE_ERROR, std::format("{}() {}.", function, message));
}

bool realValue(std::string_view function, const Value& value, double& result, NativeResult& error)
{
    if(!value.is_number())
    {
        error = typeError(function, "a real number");
        return false;
    }

    result = value.as_number();
    if(!std::isfinite(result))
    {
        error = valueError(function, "expects a finite real number");
        return false;
    }

    return true;
}

bool numericValue(std::string_view function, const Value& value, std::complex<double>& result, NativeResult& error)
{
    if(value.is_number())
        result = {value.as_number(), 0.0};
    else if(is<ObjComplex>(value))
        result = as<ObjComplex>(value)->c;
    else
    {
        error = typeError(function, "a number or complex number");
        return false;
    }

    if(!isFinite(result))
    {
        error = valueError(function, "expects finite numeric input");
        return false;
    }

    return true;
}

NativeResult realResult(std::string_view function, double value)
{
    if(!std::isfinite(value))
        return rangeError(function, "produced a non-finite real result");
    return NativeResult::success(Value(disregardZero(value)));
}

NativeResult complexResult(std::string_view function, VM& vm, const std::complex<double>& value)
{
    if(!isFinite(value))
        return rangeError(function, "produced a non-finite complex result");

    const auto normalized = disregardZero(value);
    return NativeResult::success(Value(makeObj<ObjComplex>(vm,
        normalized.real(), normalized.imag())));
}

namespace
{
NativeResult sinNative(VM&, int, Value* args)
{
    NativeResult error;
    double number = 0.0;
    if(!realValue("sin", args[0], number, error)) return error;
    return realResult("sin", std::sin(number));
}

NativeResult cosNative(VM&, int, Value* args)
{
    NativeResult error;
    double number = 0.0;
    if(!realValue("cos", args[0], number, error)) return error;
    return realResult("cos", std::cos(number));
}

NativeResult tanNative(VM&, int, Value* args)
{
    NativeResult error;
    double number = 0.0;
    if(!realValue("tan", args[0], number, error)) return error;
    return realResult("tan", std::tan(number));
}

NativeResult atanNative(VM&, int, Value* args)
{
    NativeResult error;
    double number = 0.0;
    if(!realValue("atan", args[0], number, error)) return error;
    return realResult("atan", std::atan(number));
}

NativeResult expNative(VM&, int, Value* args)
{
    NativeResult error;
    double number = 0.0;
    if(!realValue("exp", args[0], number, error)) return error;
    return realResult("exp", std::exp(number));
}

NativeResult sinhNative(VM&, int, Value* args)
{
    NativeResult error;
    double number = 0.0;
    if(!realValue("sinh", args[0], number, error)) return error;
    return realResult("sinh", std::sinh(number));
}

NativeResult coshNative(VM&, int, Value* args)
{
    NativeResult error;
    double number = 0.0;
    if(!realValue("cosh", args[0], number, error)) return error;
    return realResult("cosh", std::cosh(number));
}

NativeResult tanhNative(VM&, int, Value* args)
{
    NativeResult error;
    double number = 0.0;
    if(!realValue("tanh", args[0], number, error)) return error;
    return realResult("tanh", std::tanh(number));
}

NativeResult asinhNative(VM&, int, Value* args)
{
    NativeResult error;
    double number = 0.0;
    if(!realValue("asinh", args[0], number, error)) return error;
    return realResult("asinh", std::asinh(number));
}

NativeResult sqrtNative(VM&, int, Value* args)
{
    NativeResult error;
    double number = 0.0;
    if(!realValue("sqrt", args[0], number, error)) return error;
    if(number < 0.0)
        return domainError("sqrt", "is undefined for negative real numbers");
    return realResult("sqrt", std::sqrt(number));
}

NativeResult asinNative(VM&, int, Value* args)
{
    NativeResult error;
    double number = 0.0;
    if(!realValue("asin", args[0], number, error)) return error;
    if(number < -1.0 || number > 1.0)
        return domainError("asin", "is only defined for inputs in [-1, 1]");
    return realResult("asin", std::asin(number));
}

NativeResult acosNative(VM&, int, Value* args)
{
    NativeResult error;
    double number = 0.0;
    if(!realValue("acos", args[0], number, error)) return error;
    if(number < -1.0 || number > 1.0)
        return domainError("acos", "is only defined for inputs in [-1, 1]");
    return realResult("acos", std::acos(number));
}

NativeResult powNative(VM&, int, Value* args)
{
    NativeResult error;
    double number = 0.0;
    double exponent = 0.0;
    if(!realValue("pow", args[0], number, error) ||
        !realValue("pow", args[1], exponent, error)) return error;
    if(number < 0.0 && std::floor(exponent) != exponent)
        return domainError("pow", "is undefined for a negative base with a non-integer exponent");
    return realResult("pow", std::pow(number, exponent));
}

NativeResult lnNative(VM&, int, Value* args)
{
    NativeResult error;
    double number = 0.0;
    if(!realValue("ln", args[0], number, error)) return error;
    if(number <= 0.0)
        return domainError("ln", "is only defined for positive real numbers");
    return realResult("ln", std::log(number));
}

NativeResult lgNative(VM&, int, Value* args)
{
    NativeResult error;
    double number = 0.0;
    if(!realValue("lg", args[0], number, error)) return error;
    if(number <= 0.0)
        return domainError("lg", "is only defined for positive real numbers");
    return realResult("lg", std::log10(number));
}

NativeResult logNative(VM&, int, Value* args)
{
    NativeResult error;
    double number = 0.0;
    double base = 0.0;
    if(!realValue("log", args[0], number, error) ||
        !realValue("log", args[1], base, error)) return error;

    number = disregardZero(number);
    base = disregardZero(base);
    if(number <= 0.0)
        return domainError("log", "number cannot be negative or 0");
    if(base <= 0.0)
        return domainError("log", "base cannot be negative or 0");
    if(base == 1.0)
        return domainError("log", "base cannot be 1");
    return realResult("log", std::log(number) / std::log(base));
}

NativeResult acoshNative(VM&, int, Value* args)
{
    NativeResult error;
    double number = 0.0;
    if(!realValue("acosh", args[0], number, error)) return error;
    if(number < 1.0)
        return domainError("acosh", "is only defined for inputs >= 1");
    return realResult("acosh", std::acosh(number));
}

NativeResult atanhNative(VM&, int, Value* args)
{
    NativeResult error;
    double number = 0.0;
    if(!realValue("atanh", args[0], number, error)) return error;
    if(number <= -1.0 || number >= 1.0)
        return domainError("atanh", "is only defined for inputs in (-1, 1)");
    return realResult("atanh", std::atanh(number));
}

NativeResult sinCxNative(VM& vm, int, Value* args)
{
    NativeResult error;
    std::complex<double> number;
    if(!numericValue("sin'", args[0], number, error)) return error;
    return complexResult("sin'", vm, std::sin(number));
}

NativeResult cosCxNative(VM& vm, int, Value* args)
{
    NativeResult error;
    std::complex<double> number;
    if(!numericValue("cos'", args[0], number, error)) return error;
    return complexResult("cos'", vm, std::cos(number));
}

NativeResult tanCxNative(VM& vm, int, Value* args)
{
    NativeResult error;
    std::complex<double> number;
    if(!numericValue("tan'", args[0], number, error)) return error;
    return complexResult("tan'", vm, std::tan(number));
}

NativeResult asinCxNative(VM& vm, int, Value* args)
{
    NativeResult error;
    std::complex<double> number;
    if(!numericValue("asin'", args[0], number, error)) return error;
    return complexResult("asin'", vm, std::asin(number));
}

NativeResult acosCxNative(VM& vm, int, Value* args)
{
    NativeResult error;
    std::complex<double> number;
    if(!numericValue("acos'", args[0], number, error)) return error;
    return complexResult("acos'", vm, std::acos(number));
}

NativeResult atanCxNative(VM& vm, int, Value* args)
{
    NativeResult error;
    std::complex<double> number;
    if(!numericValue("atan'", args[0], number, error)) return error;
    return complexResult("atan'", vm, std::atan(number));
}

NativeResult expCxNative(VM& vm, int, Value* args)
{
    NativeResult error;
    std::complex<double> number;
    if(!numericValue("exp'", args[0], number, error)) return error;
    return complexResult("exp'", vm, std::exp(number));
}

NativeResult sinhCxNative(VM& vm, int, Value* args)
{
    NativeResult error;
    std::complex<double> number;
    if(!numericValue("sinh'", args[0], number, error)) return error;
    return complexResult("sinh'", vm, std::sinh(number));
}

NativeResult coshCxNative(VM& vm, int, Value* args)
{
    NativeResult error;
    std::complex<double> number;
    if(!numericValue("cosh'", args[0], number, error)) return error;
    return complexResult("cosh'", vm, std::cosh(number));
}

NativeResult tanhCxNative(VM& vm, int, Value* args)
{
    NativeResult error;
    std::complex<double> number;
    if(!numericValue("tanh'", args[0], number, error)) return error;
    return complexResult("tanh'", vm, std::tanh(number));
}

NativeResult asinhCxNative(VM& vm, int, Value* args)
{
    NativeResult error;
    std::complex<double> number;
    if(!numericValue("asinh'", args[0], number, error)) return error;
    return complexResult("asinh'", vm, std::asinh(number));
}

NativeResult acoshCxNative(VM& vm, int, Value* args)
{
    NativeResult error;
    std::complex<double> number;
    if(!numericValue("acosh'", args[0], number, error)) return error;
    return complexResult("acosh'", vm, std::acosh(number));
}

NativeResult atanhCxNative(VM& vm, int, Value* args)
{
    NativeResult error;
    std::complex<double> number;
    if(!numericValue("atanh'", args[0], number, error)) return error;
    return complexResult("atanh'", vm, std::atanh(number));
}

NativeResult sqrtCxNative(VM& vm, int, Value* args)
{
    NativeResult error;
    std::complex<double> number;
    if(!numericValue("sqrt'", args[0], number, error)) return error;
    return complexResult("sqrt'", vm, std::sqrt(number));
}

NativeResult realNative(VM&, int, Value* args)
{
    NativeResult error;
    std::complex<double> number;
    if(!numericValue("real", args[0], number, error)) return error;
    return realResult("real", number.real());
}

NativeResult imagNative(VM&, int, Value* args)
{
    NativeResult error;
    std::complex<double> number;
    if(!numericValue("imag", args[0], number, error)) return error;
    return realResult("imag", number.imag());
}

NativeResult absNative(VM&, int, Value* args)
{
    NativeResult error;
    std::complex<double> number;
    if(!numericValue("abs", args[0], number, error)) return error;
    return realResult("abs", std::abs(number));
}

NativeResult argNative(VM&, int, Value* args)
{
    NativeResult error;
    std::complex<double> number;
    if(!numericValue("arg", args[0], number, error)) return error;
    return realResult("arg", std::arg(number));
}

NativeResult normNative(VM&, int, Value* args)
{
    NativeResult error;
    std::complex<double> number;
    if(!numericValue("mag", args[0], number, error)) return error;
    return realResult("mag", std::norm(number));
}

NativeResult conjNative(VM& vm, int, Value* args)
{
    NativeResult error;
    std::complex<double> number;
    if(!numericValue("conj", args[0], number, error)) return error;
    if(args[0].is_number()) return NativeResult::success(args[0]);
    return complexResult("conj", vm, std::conj(number));
}

NativeResult floorNative(VM&, int, Value* args)
{
    NativeResult error;
    double number = 0.0;
    if(!realValue("floor", args[0], number, error)) return error;
    return realResult("floor", std::floor(number));
}

NativeResult ceilNative(VM&, int, Value* args)
{
    NativeResult error;
    double number = 0.0;
    if(!realValue("ceil", args[0], number, error)) return error;
    return realResult("ceil", std::ceil(number));
}

NativeResult erfNative(VM&, int, Value* args)
{
    NativeResult error;
    double number = 0.0;
    if(!realValue("erf", args[0], number, error)) return error;
    return realResult("erf", statcpp::erf(number));
}

NativeResult erfcNative(VM&, int, Value* args)
{
    NativeResult error;
    double number = 0.0;
    if(!realValue("erfc", args[0], number, error)) return error;
    return realResult("erfc", statcpp::erfc(number));
}

NativeResult gammaNative(VM&, int, Value* args)
{
    NativeResult error;
    double number = 0.0;
    if(!realValue("gamma", args[0], number, error)) return error;
    if(number <= 0.0 && number == std::floor(number))
        return domainError("gamma", "is undefined for non-positive integers");
    return realResult("gamma", statcpp::tgamma(number));
}


// -----

constexpr std::array definitions {
    NativeDef{"sqrt", 1, sqrtNative}, 
    NativeDef{"sin", 1, sinNative},
    NativeDef{"cos", 1, cosNative}, 
    NativeDef{"tan", 1, tanNative},
    NativeDef{"asin", 1, asinNative}, 
    NativeDef{"acos", 1, acosNative},
    NativeDef{"atan", 1, atanNative}, 
    NativeDef{"exp", 1, expNative},
    NativeDef{"pow", 2, powNative}, 
    NativeDef{"ln", 1, lnNative},
    NativeDef{"lg", 1, lgNative}, 
    NativeDef{"log", 2, logNative},
    NativeDef{"sinh", 1, sinhNative}, 
    NativeDef{"cosh", 1, coshNative},
    NativeDef{"tanh", 1, tanhNative}, 
    NativeDef{"asinh", 1, asinhNative},
    NativeDef{"acosh", 1, acoshNative}, 
    NativeDef{"atanh", 1, atanhNative},

    NativeDef{"sqrt'", 1, sqrtCxNative}, 
    NativeDef{"real", 1, realNative},
    NativeDef{"imag", 1, imagNative}, 
    NativeDef{"abs", 1, absNative},
    NativeDef{"arg", 1, argNative}, 
    NativeDef{"mag", 1, normNative},
    NativeDef{"conj", 1, conjNative},
    
    NativeDef{"sin'", 1, sinCxNative},
    NativeDef{"cos'", 1, cosCxNative}, 
    NativeDef{"tan'", 1, tanCxNative},
    NativeDef{"exp'", 1, expCxNative}, 
    NativeDef{"sinh'", 1, sinhCxNative},
    NativeDef{"cosh'", 1, coshCxNative}, 
    NativeDef{"tanh'", 1, tanhCxNative},
    NativeDef{"asin'", 1, asinCxNative}, 
    NativeDef{"acos'", 1, acosCxNative},
    NativeDef{"atan'", 1, atanCxNative}, 
    NativeDef{"asinh'", 1, asinhCxNative},
    NativeDef{"acosh'", 1, acoshCxNative}, 
    NativeDef{"atanh'", 1, atanhCxNative},

    NativeDef{"floor", 1, floorNative},
    NativeDef{"ceil", 1, ceilNative},

    NativeDef{"erf", 1, erfNative},
    NativeDef{"erfc", 1, erfcNative},
    NativeDef{"gamma", 1, gammaNative},
};
}

std::span<const NativeDef> mathNativeDefinitions() noexcept
{
    return definitions;
}
