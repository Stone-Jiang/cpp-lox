#include "native.h"
#include "object.h"

#include <array>
#include <ctime>

namespace
{
NativeResult clockNative(VM&, int, Value*)
{
    return NativeResult::success(
        Value(static_cast<double>(std::clock()) / CLOCKS_PER_SEC));
}

NativeResult typeNative(VM& vm, int, Value* args)
{
    std::string_view type;

    if(args[0].is_number())
        type = "number";
    else if(args[0].is_nil())
        type = "nil";
    else if(args[0].is_bool())
        type = "bool";
    else
    {
        switch(objType(args[0]))
        {
        case ObjType::FUNCTION:
        case ObjType::CLOSURE:
        case ObjType::NATIVE:
        case ObjType::BOUND_METHOD:
            type = "fun";
            break;
        case ObjType::ERROR:
            type = "error";
            break;
        case ObjType::CLASS:
            type = "class";
            break;
        case ObjType::INSTANCE:
            return NativeResult::success(Value(
                copyString(vm, as<ObjInstance>(args[0])->klass->name->str())));
        default:
            type = "object";
            break;
        }
    }

    return NativeResult::success(Value(copyString(vm, type)));
}

constexpr std::array definitions {
    NativeDef{"clock", 0, clockNative},
    NativeDef{"typeof", 1, typeNative},
};

}

std::span<const NativeDef> nativeDefinitions() noexcept
{
    return definitions;
}
