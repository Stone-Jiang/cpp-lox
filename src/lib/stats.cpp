#include "stats.h"
#include "math.h"
#include "../utils/special.hpp"

namespace
{
NativeResult normCdfNative(VM&, int, Value* args)
{
    NativeResult error;
    double number = 0.0;
    if(!realValue("norm_cdf", args[0], number, error)) return error;
    return realResult("norm_cdf", statcpp::norm_cdf(number));
}

NativeResult normInvNative(VM&, int, Value* args)
{
    NativeResult error;
    double number = 0.0;
    if(!realValue("norm_inv", args[0], number, error)) return error;
    if(number <= 0.0 || number >= 1.0)
        return domainError("norm_inv", "is only defined for probabilities in (0, 1)");
    return realResult("norm_inv", statcpp::norm_quantile(number));
}

constexpr std::array definitions {
    NativeDef{"norm_cdf", 1, normCdfNative}, 
    NativeDef{"norm_inv", 1, normInvNative},
};

}


std::span<const NativeDef> statsNativeDefinitions() noexcept
{
    return definitions;
}
