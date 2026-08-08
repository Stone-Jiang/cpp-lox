#include "native.h"

#include <array>
#include <ctime>
#include <cmath>

namespace
{
Value clockNative(int, Value*)
{
    return Value(static_cast<double>(std::clock()) / CLOCKS_PER_SEC);
}

Value sqrtNative(int, Value* args)
{
    if(!args[0].is_number())
        return Value();

    const double number = args[0].as_number();
    if(number < 0)
        return Value();

    return Value(std::sqrt(number));
}



constexpr std::array definitions {
    NativeDef{"clock", 0, clockNative},
    NativeDef{"sqrt", 1, sqrtNative},
};

}

std::span<const NativeDef> nativeDefinitions() noexcept
{
    return definitions;
}
