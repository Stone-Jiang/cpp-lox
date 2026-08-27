#include "math.h"

#include "../lang/object.h"
#include "../lang/vm.h"

#include <array>
#include <cmath>
#include <complex>

namespace
{
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

NativeResult success(Value value)
{
    return NativeResult::success(value);
}

NativeResult failure(ErrorKind kind, std::string_view message, Value payload = Value())
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
    return failure(ErrorKind::VALUE_ERROR,
        std::format("{}() {}.", function, message));
}

NativeResult domainError(std::string_view function, std::string_view message)
{
    return failure(ErrorKind::DOMAIN_ERROR,
        std::format("{}() {}.", function, message));
}

NativeResult rangeError(std::string_view function, std::string_view message)
{
    return failure(ErrorKind::RANGE_ERROR,
        std::format("{}() {}.", function, message));
}

bool realValue(std::string_view function, const Value& value,
    double& result, NativeResult& error)
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

bool numericValue(std::string_view function, const Value& value,
    std::complex<double>& result, NativeResult& error)
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
    return success(Value(disregardZero(value)));
}

NativeResult complexResult(std::string_view function, VM& vm,
    const std::complex<double>& value)
{
    if(!isFinite(value))
        return rangeError(function, "produced a non-finite complex result");

    const auto normalized = disregardZero(value);
    return success(Value(makeObj<ObjComplex>(vm,
        normalized.real(), normalized.imag())));
}

#define REAL_UNARY(NAME, EXPRESSION) \
NativeResult NAME##Native(VM&, int, Value* args) \
{ \
    NativeResult error; \
    double number = 0.0; \
    if(!realValue(#NAME, args[0], number, error)) return error; \
    return realResult(#NAME, (EXPRESSION)); \
}

REAL_UNARY(sin, std::sin(number))
REAL_UNARY(cos, std::cos(number))
REAL_UNARY(tan, std::tan(number))
REAL_UNARY(atan, std::atan(number))
REAL_UNARY(exp, std::exp(number))
REAL_UNARY(sinh, std::sinh(number))
REAL_UNARY(cosh, std::cosh(number))
REAL_UNARY(tanh, std::tanh(number))
REAL_UNARY(asinh, std::asinh(number))

#undef REAL_UNARY

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

#define COMPLEX_UNARY(NAME, EXPRESSION) \
NativeResult NAME##CxNative(VM& vm, int, Value* args) \
{ \
    NativeResult error; \
    std::complex<double> number; \
    if(!numericValue(#NAME "'", args[0], number, error)) return error; \
    return complexResult(#NAME "'", vm, (EXPRESSION)); \
}

COMPLEX_UNARY(sin, std::sin(number))
COMPLEX_UNARY(cos, std::cos(number))
COMPLEX_UNARY(tan, std::tan(number))
COMPLEX_UNARY(asin, std::asin(number))
COMPLEX_UNARY(acos, std::acos(number))
COMPLEX_UNARY(atan, std::atan(number))
COMPLEX_UNARY(exp, std::exp(number))
COMPLEX_UNARY(sinh, std::sinh(number))
COMPLEX_UNARY(cosh, std::cosh(number))
COMPLEX_UNARY(tanh, std::tanh(number))
COMPLEX_UNARY(asinh, std::asinh(number))
COMPLEX_UNARY(acosh, std::acosh(number))
COMPLEX_UNARY(atanh, std::atanh(number))

#undef COMPLEX_UNARY

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
    if(args[0].is_number()) return success(args[0]);
    return complexResult("conj", vm, std::conj(number));
}

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
};
}

std::span<const NativeDef> mathNativeDefinitions() noexcept
{
    return definitions;
}
