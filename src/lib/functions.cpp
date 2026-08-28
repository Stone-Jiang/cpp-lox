#include "functions.h"

#include "../lang/object.h"

#include <array>

namespace
{
NativeResult functionArity(VM&, Value receiver)
{
    int arity = 0;
    switch(objType(receiver))
    {
    case ObjType::CLOSURE:
        arity = as<ObjClosure>(receiver)->func->arity;
        break;
    case ObjType::NATIVE:
        arity = as<ObjNative>(receiver)->arity;
        break;
    case ObjType::BOUND_METHOD:
        arity = as<ObjBoundMethod>(receiver)->method->func->arity;
        break;
    default:
        return NativeResult::failure(
            ErrorKind::TYPE_ERROR,
            "Only functions have an arity.",
            receiver);
    }

    return NativeResult::success(Value(static_cast<double>(arity)));
}

constexpr std::array properties {
    NativePropertyDef{"arity", functionArity},
};

constexpr std::array<NativeMethodDef, 0> methods {};
}

std::span<const NativePropertyDef> functionNativeProperties() noexcept
{
    return properties;
}

std::span<const NativeMethodDef> functionNativeMethods() noexcept
{
    return methods;
}
