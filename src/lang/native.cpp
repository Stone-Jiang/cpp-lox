#include "native.h"
#include "../lib/arrays.h"
#include "object.h"

#include <array>
#include <ctime>

NativeResult NativeResult::success(Value value)
{
    NativeResult result;
    result.value = value;
    return result;
}

NativeResult NativeResult::failure(ErrorKind kind, std::string message, Value payload)
{
    NativeResult result;
    result.ok = false;
    result.error = NativeError{kind, std::move(message), payload};
    return result;
}

NativeResult NativeResult::failure(const ObjError* error)
{
    NativeResult result;
    result.ok = false;
    result.error = NativeError{
        error->kind,
        error->message == nullptr ? std::string{} : error->message->str(),
        error->payload};
    return result;
}

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
        case ObjType::ARRAY:
            type = "array";
            break;
        default:
            type = "object";
            break;
        }
    }

    return NativeResult::success(Value(copyString(vm, type)));
}

NativeResult strNative(VM& vm, int, Value* args)
{
    return NativeResult::success(Value(copyString(vm, to_string(args[0]))));
}

constexpr std::array definitions {
    NativeDef{"clock", 0, clockNative},
    NativeDef{"typeof", 1, typeNative},
    NativeDef{"str", 1, strNative},
};

}

std::span<const NativeDef> nativeDefinitions() noexcept
{
    return definitions;
}

std::span<const NativeTypeDef> nativeTypeDefinitions() noexcept
{
    static const std::array types {
        NativeTypeDef{
            ObjType::ARRAY,
            arrayNativeProperties(),
            arrayNativeMethods()},
    };
    return types;
}

const NativePropertyDef* findNativeProperty(
    ObjType type, std::string_view name) noexcept
{
    for(const auto& nativeType: nativeTypeDefinitions())
    {
        if(nativeType.type != type)
            continue;
        for(const auto& property: nativeType.properties)
        {
            if(property.name == name)
                return &property;
        }
        return nullptr;
    }
    return nullptr;
}

const NativeMethodDef* findNativeMethod(
    ObjType type, std::string_view name) noexcept
{
    for(const auto& nativeType: nativeTypeDefinitions())
    {
        if(nativeType.type != type)
            continue;
        for(const auto& method: nativeType.methods)
        {
            if(method.name == name)
                return &method;
        }
        return nullptr;
    }
    return nullptr;
}

bool hasNativeType(ObjType type) noexcept
{
    for(const auto& nativeType: nativeTypeDefinitions())
    {
        if(nativeType.type == type)
            return true;
    }
    return false;
}
