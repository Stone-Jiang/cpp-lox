#include "native.h"
#include "vm.h"

#include <array>
#include <cmath>
#include <complex>
#include <ctime>

namespace
{
constexpr double ZERO_EPSILON = 1e-9;

double disregardZero(double value)
{
    return std::abs(value) < ZERO_EPSILON ? 0.0 : value;
}

std::complex<double> disregardZero(std::complex<double> value)
{
    return {
        disregardZero(value.real()),
        disregardZero(value.imag())
    };
}

bool numericValue(const Value& value, std::complex<double>& result)
{
    if(value.is_number())
    {
        result = {value.as_number(), 0.0};
        return true;
    }
    if(is_complex(value))
    {
        result = as_complex(value)->c;
        return true;
    }
    return false;
}

Value complexValue(VM& vm, const std::complex<double>& value)
{
    const auto normalized = disregardZero(value);
    return Value(makeObj<ObjComplex>(vm,
        normalized.real(), normalized.imag()));
}

Value clockNative(VM&, int, Value*)
{
    return Value(static_cast<double>(std::clock()) / CLOCKS_PER_SEC);
}

Value sqrtNative(VM& vm, int, Value* args)
{
    if(!args[0].is_number())
    {
        vm.reportRuntimeError("sqrt() expects a real number.");
        return Value();
    }

    const double number = args[0].as_number();
    if(number < 0.0)
    {
        vm.reportRuntimeError("sqrt() is undefined for negative real numbers.");
        return Value();
    }

    return Value(disregardZero(std::sqrt(number)));
}

Value sinNative(VM& vm, int, Value* args)
{
    if(!args[0].is_number())
    {
        vm.reportRuntimeError("sin() expects a real number.");
        return Value();
    }

    const double number = args[0].as_number();
    return Value(disregardZero(std::sin(number)));
}

Value cosNative(VM& vm, int, Value* args)
{
    if(!args[0].is_number())
    {
        vm.reportRuntimeError("cos() expects a real number.");
        return Value();
    }

    const double number = args[0].as_number();
    return Value(disregardZero(std::cos(number)));
}

Value tanNative(VM& vm, int, Value* args)
{
    if(!args[0].is_number())
    {
        vm.reportRuntimeError("tan() expects a real number.");
        return Value();
    }

    const double number = args[0].as_number();
    return Value(disregardZero(std::tan(number)));
}

Value asinNative(VM& vm, int, Value* args)
{
    if(!args[0].is_number())
    {
        vm.reportRuntimeError("sin() expects a real number.");
        return Value();
    }

    const double number = args[0].as_number();
    return Value(disregardZero(std::asin(number)));
}

Value acosNative(VM& vm, int, Value* args)
{
    if(!args[0].is_number())
    {
        vm.reportRuntimeError("sin() expects a real number.");
        return Value();
    }

    const double number = args[0].as_number();
    return Value(disregardZero(std::acos(number)));
}

Value atanNative(VM& vm, int, Value* args)
{
    if(!args[0].is_number())
    {
        vm.reportRuntimeError("atan() expects a real number.");
        return Value();
    }

    const double number = args[0].as_number();
    return Value(disregardZero(std::atan(number)));
}

Value expNative(VM& vm, int, Value* args)
{
    if(!args[0].is_number())
    {
        vm.reportRuntimeError("exp() expects a real number.");
        return Value();
    }

    const double number = args[0].as_number();
    return Value(disregardZero(std::exp(number)));
}

Value powNative(VM& vm, int, Value* args)
{
    if(!args[0].is_number() || !args[1].is_number())
    {
        vm.reportRuntimeError("pow() expects a real number.");
        return Value();
    }

    const double number = args[0].as_number();
    const double ind = args[1].as_number();
    return Value(disregardZero(std::pow(number, ind)));
}

Value lnNative(VM& vm, int, Value* args)
{
    if(!args[0].is_number())
    {
        vm.reportRuntimeError("ln() expects a real number.");
        return Value();
    }

    const double number = args[0].as_number();
    return Value(disregardZero(std::log(number)));
}

Value lgNative(VM& vm, int, Value* args)
{
    if(!args[0].is_number())
    {
        vm.reportRuntimeError("lg() expects a real number.");
        return Value();
    }

    const double number = args[0].as_number();
    return Value(disregardZero(std::log10(number)));
}

Value logNative(VM& vm, int, Value* args)
{
    if(!args[0].is_number() || !args[1].is_number())
    {
        vm.reportRuntimeError("log() expects a real number.");
        return Value();
    }

    const double number = disregardZero(args[0].as_number());
    const double base = disregardZero(args[1].as_number());

    if(number <= 0)
        vm.reportRuntimeError("log() number cannot be negative or 0.");
    if(base <= 0)
        vm.reportRuntimeError("log() base cannot be negative or 0.");
    if(base == 1.0)
        vm.reportRuntimeError("log() base cannot be 1.");

    return Value(disregardZero(std::log(number) / std::log(base)));
}

Value sinhNative(VM& vm, int, Value* args)
{
    if(!args[0].is_number())
    {
        vm.reportRuntimeError("sinh() expects a real number.");
        return Value();
    }

    const double number = args[0].as_number();
    return Value(disregardZero(std::sinh(number)));
}

Value coshNative(VM& vm, int, Value* args)
{
    if(!args[0].is_number())
    {
        vm.reportRuntimeError("cosh() expects a real number.");
        return Value();
    }

    const double number = args[0].as_number();
    return Value(disregardZero(std::cosh(number)));
}

Value tanhNative(VM& vm, int, Value* args)
{
    if(!args[0].is_number())
    {
        vm.reportRuntimeError("tanh() expects a real number.");
        return Value();
    }

    const double number = args[0].as_number();
    return Value(disregardZero(std::tanh(number)));
}

Value asinhNative(VM& vm, int, Value* args)
{
    if(!args[0].is_number())
    {
        vm.reportRuntimeError("asinh() expects a real number.");
        return Value();
    }

    const double number = args[0].as_number();
    return Value(disregardZero(std::asinh(number)));
}

Value acoshNative(VM& vm, int, Value* args)
{
    if(!args[0].is_number())
    {
        vm.reportRuntimeError("acosh() expects a real number.");
        return Value();
    }

    const double number = args[0].as_number();
    return Value(disregardZero(std::acosh(number)));
}

Value atanhNative(VM& vm, int, Value* args)
{
    if(!args[0].is_number())
    {
        vm.reportRuntimeError("atanh() expects a real number.");
        return Value();
    }

    const double number = args[0].as_number();
    return Value(disregardZero(std::atanh(number)));
}


Value sqrtCxNative(VM& vm, int, Value* args)
{
    std::complex<double> number;
    if(!numericValue(args[0], number))
    {
        vm.reportRuntimeError("sqrt'() expects a number or complex number.");
        return Value();
    }

    return complexValue(vm, std::sqrt(number));
}

Value realNative(VM&, int, Value* args)
{
    std::complex<double> number;
    if(!numericValue(args[0], number))
        return Value();
    return Value(disregardZero(number.real()));
}

Value imagNative(VM&, int, Value* args)
{
    std::complex<double> number;
    if(!numericValue(args[0], number))
        return Value();
    return Value(disregardZero(number.imag()));
}

Value absNative(VM&, int, Value* args)
{
    std::complex<double> number;
    if(!numericValue(args[0], number))
        return Value();
    return Value(disregardZero(std::abs(number)));
}

Value argNative(VM&, int, Value* args)
{
    std::complex<double> number;
    if(!numericValue(args[0], number))
        return Value();
    return Value(disregardZero(std::arg(number)));
}

Value normNative(VM&, int, Value* args)
{
    std::complex<double> number;
    if(!numericValue(args[0], number))
        return Value();
    return Value(disregardZero(std::norm(number)));
}

Value conjNative(VM& vm, int, Value* args)
{
    std::complex<double> number;
    if(!numericValue(args[0], number))
        return Value();

    if(args[0].is_number())
        return args[0];
    return complexValue(vm, std::conj(number));
}

Value sinCxNative(VM& vm, int, Value* args)
{
    std::complex<double> number;
    if(!numericValue(args[0], number))
    {
        vm.reportRuntimeError("sin'() expects a number or complex number.");
        return Value();
    }

    return complexValue(vm, std::sin(number));
}

Value cosCxNative(VM& vm, int, Value* args)
{
    std::complex<double> number;
    if(!numericValue(args[0], number))
    {
        vm.reportRuntimeError("cos'() expects a number or complex number.");
        return Value();
    }

    return complexValue(vm, std::cos(number));
}

Value tanCxNative(VM& vm, int, Value* args)
{
    std::complex<double> number;
    if(!numericValue(args[0], number))
    {
        vm.reportRuntimeError("tan'() expects a number or complex number.");
        return Value();
    }

    return complexValue(vm, std::tan(number));
}

Value asinCxNative(VM& vm, int, Value* args)
{
    std::complex<double> number;
    if(!numericValue(args[0], number))
    {
        vm.reportRuntimeError("asin'() expects a number or complex number.");
        return Value();
    }

    return complexValue(vm, std::asin(number));
}

Value acosCxNative(VM& vm, int, Value* args)
{
    std::complex<double> number;
    if(!numericValue(args[0], number))
    {
        vm.reportRuntimeError("acos'() expects a number or complex number.");
        return Value();
    }

    return complexValue(vm, std::acos(number));
}

Value atanCxNative(VM& vm, int, Value* args)
{
    std::complex<double> number;
    if(!numericValue(args[0], number))
    {
        vm.reportRuntimeError("atan'() expects a number or complex number.");
        return Value();
    }

    return complexValue(vm, std::atan(number));
}

Value expCxNative(VM& vm, int, Value* args)
{
    std::complex<double> number;
    if(!numericValue(args[0], number))
    {
        vm.reportRuntimeError("exp'() expects a number or complex number.");
        return Value();
    }

    return complexValue(vm, std::exp(number));
}

Value sinhCxNative(VM& vm, int, Value* args)
{
    std::complex<double> number;
    if(!numericValue(args[0], number))
    {
        vm.reportRuntimeError("sin'() expects a number or complex number.");
        return Value();
    }

    return complexValue(vm, std::sinh(number));
}

Value coshCxNative(VM& vm, int, Value* args)
{
    std::complex<double> number;
    if(!numericValue(args[0], number))
    {
        vm.reportRuntimeError("cos'() expects a number or complex number.");
        return Value();
    }

    return complexValue(vm, std::cosh(number));
}

Value tanhCxNative(VM& vm, int, Value* args)
{
    std::complex<double> number;
    if(!numericValue(args[0], number))
    {
        vm.reportRuntimeError("tan'() expects a number or complex number.");
        return Value();
    }

    return complexValue(vm, std::tanh(number));
}

Value asinhCxNative(VM& vm, int, Value* args)
{
    std::complex<double> number;
    if(!numericValue(args[0], number))
    {
        vm.reportRuntimeError("sin'() expects a number or complex number.");
        return Value();
    }

    return complexValue(vm, std::asinh(number));
}

Value acoshCxNative(VM& vm, int, Value* args)
{
    std::complex<double> number;
    if(!numericValue(args[0], number))
    {
        vm.reportRuntimeError("cos'() expects a number or complex number.");
        return Value();
    }

    return complexValue(vm, std::acosh(number));
}

Value atanhCxNative(VM& vm, int, Value* args)
{
    std::complex<double> number;
    if(!numericValue(args[0], number))
    {
        vm.reportRuntimeError("tan'() expects a number or complex number.");
        return Value();
    }

    return complexValue(vm, std::atanh(number));
}



// -----

constexpr std::array definitions {
    NativeDef{"clock", 0, clockNative},

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
};

}

std::span<const NativeDef> nativeDefinitions() noexcept
{
    return definitions;
}
