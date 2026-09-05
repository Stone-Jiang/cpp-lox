#include "native.h"
#include "../lib/arrays.h"
#include "../lib/functions.h"
#include "../lib/files.h"
#include "../lib/maps.h"
#include "../lib/strings.h"
#include "../lib/refl.h"
#include "object.h"
#include "vm.h"

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
    return NativeResult::success(Value(static_cast<double>(std::clock()) / CLOCKS_PER_SEC));
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
        case ObjType::MAP:
            type = "map";
            break;
        case ObjType::FILE:
            type = "file";
            break;
        case ObjType::STRING:
            type = "string";
            break;
        case ObjType::COMPLEX:
            type = "complex";
            break;
        case ObjType::UPVALUE:
            type = "upvalue";
            break;
        case ObjType::OBJ:
            type = "obj?";
            break;
        case ObjType::NONE:
            type = "none?";
            break;
        }
    }

    return NativeResult::success(Value(copyString(vm, type)));
}

NativeResult strNative(VM& vm, int, Value* args)
{
    return NativeResult::success(Value(copyString(vm, to_string(args[0]))));
}

NativeResult integralNative(VM&, int, Value* args)
{
    return NativeResult::success(Value(is_integral(args[0])));
}

NativeResult systemNative(VM&, int, Value* args)
{
    if(!is<ObjString>(args[0]))
        return NativeResult::failure(ErrorKind::TYPE_ERROR, "system() expects a string.");

    system(as_cstr(args[0]));
    return NativeResult::success(Value());
}

NativeResult stodNative(VM&, int, Value* args)
{
    if(!is<ObjString>(args[0]))
        return NativeResult::failure(ErrorKind::TYPE_ERROR, "stod() expects a string.", Value(0.0));

    try 
    {
        double temp = std::stod(as<ObjString>(args[0])->str());
        return NativeResult::success(Value(temp));
    }
    catch(const std::invalid_argument&)
    {
        return NativeResult::failure(ErrorKind::VALUE_ERROR, "This string cannot be converted to number.", Value(0.0));
    }
    catch(const std::out_of_range&)
    {
        return NativeResult::failure(ErrorKind::VALUE_ERROR, "This string exceeds the limits of number.", Value(0.0));
    }
}

NativeResult inputNative(VM& vm, int, Value* args)
{   
    if(!is<ObjString>(args[0]))
        return NativeResult::failure(ErrorKind::TYPE_ERROR, "input() expects a string as argument.");

    const auto& prompt = as<ObjString>(args[0])->chars;
    if(!prompt.empty())
        std::cout<<prompt<<std::flush;

    std::string line;
    if(!std::getline(std::cin, line))
        return NativeResult::failure(ErrorKind::IO_ERROR, "input() receives no input.");

    return NativeResult::success(Value(copyString(vm, line)));
}

constexpr std::array definitions {
    NativeDef{"clock", 0, clockNative},
    NativeDef{"typeof", 1, typeNative},
    NativeDef{"str", 1, strNative},
    NativeDef{"integral", 1, integralNative},
    NativeDef{"system", 1, systemNative},
    NativeDef{"stod", 1, stodNative},
    NativeDef{"input", 1, inputNative},
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
        NativeTypeDef{
            ObjType::STRING,
            stringNativeProperties(),
            stringNativeMethods()},
        NativeTypeDef{
            ObjType::FILE,
            fileNativeProperties(),
            fileNativeMethods()},
        NativeTypeDef{
            ObjType::MAP,
            mapNativeProperties(),
            mapNativeMethods()},
        NativeTypeDef{
            ObjType::CLOSURE,
            functionNativeProperties(),
            functionNativeMethods()},
        NativeTypeDef{
            ObjType::NATIVE,
            functionNativeProperties(),
            functionNativeMethods()},
        NativeTypeDef{
            ObjType::BOUND_METHOD,
            functionNativeProperties(),
            functionNativeMethods()},
        NativeTypeDef{
            ObjType::INSTANCE,
            instanceNativeProperties(),
            instanceNativeMethods()},
        NativeTypeDef{
            ObjType::CLASS,
            classNativeProperties(),
            classNativeMethods()},
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
