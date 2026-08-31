#include "strings.h"

#include "../lang/object.h"
#include "../lang/vm.h"

#include <algorithm>
#include <array>
#include <cctype>
#include <new>
#include <stdexcept>

NativeResult makeStringResult(VM& vm, std::string_view value)
{
    try
    {
        return NativeResult::success(Value(copyString(vm, value)));
    }
    catch(const std::bad_alloc&)
    {
        return NativeResult::failure(ErrorKind::CRITICAL_ERROR, "Not enough memory to create string.");
    }
    catch(const std::length_error&)
    {
        return NativeResult::failure(ErrorKind::CRITICAL_ERROR, "String result is too large.");
    }
}

namespace
{
NativeResult makeIndexError(Value index, size_t length, IndexResult reason)
{
    switch(reason)
    {
    case IndexResult::NOT_NUMBER:
        return NativeResult::failure(ErrorKind::INDEX_ERROR, "String index must be an integral number.", index);
    case IndexResult::NOT_FINITE:
        return NativeResult::failure(ErrorKind::INDEX_ERROR, "String index must be finite.", index);
    case IndexResult::NOT_INTEGRAL:
        return NativeResult::failure(ErrorKind::INDEX_ERROR, "String index must be integral.", index);
    case IndexResult::NOT_SAFE:
        return NativeResult::failure(ErrorKind::INDEX_ERROR, "String index is outside the integer range.", index);
    case IndexResult::OUT_OF_RANGE:
    case IndexResult::END:
        return NativeResult::failure(ErrorKind::INDEX_ERROR, std::format("String index is out of range for length {}.", length), index);
    case IndexResult::OK:
    default:
        return NativeResult::success(Value());
    }
}

template<typename Transform>
NativeResult transformString(VM& vm, Value receiver, Transform transform)
{
    try
    {
        // Always transform independent storage. The immutable receiver is
        // never exposed to a method as writable memory.
        std::string result = as<ObjString>(receiver)->str();
        transform(result);
        return makeStringResult(vm, result);
    }
    catch(const std::bad_alloc&)
    {
        return NativeResult::failure(ErrorKind::CRITICAL_ERROR,"Not enough memory to transform string.");
    }
    catch(const std::length_error&)
    {
        return NativeResult::failure(ErrorKind::CRITICAL_ERROR, "String result is too large.");
    }
}

NativeResult stringLength(VM&, Value receiver)
{
    const size_t length = as<ObjString>(receiver)->str().size();
    return NativeResult::success(Value(static_cast<double>(length)));
}

NativeResult stringUpper(VM& vm, Value receiver, int, Value*)
{
    return transformString(vm, receiver, [](std::string& result) {
        std::transform(result.begin(), result.end(), result.begin(),
            [](unsigned char c) {
                return static_cast<char>(std::toupper(c));
            });
    });
}

NativeResult stringLower(VM& vm, Value receiver, int, Value*)
{
    return transformString(vm, receiver, [](std::string& result) {
        std::transform(result.begin(), result.end(), result.begin(),
            [](unsigned char c) {
                return static_cast<char>(std::tolower(c));
            });
    });
}

NativeResult stringTrim(VM& vm, Value receiver, int, Value*)
{
    return transformString(vm, receiver, [](std::string& result) {
        auto first = std::find_if_not(result.begin(), result.end(),
            [](unsigned char c) {return std::isspace(c) != 0;});
        auto last = std::find_if_not(result.rbegin(), result.rend(),
            [](unsigned char c) {return std::isspace(c) != 0;}).base();

        if(first >= last)
        {
            result.clear();
            return;
        }
        result = std::string(first, last);
    });
}

NativeResult stringReverse(VM& vm, Value receiver, int, Value*)
{
    return transformString(vm, receiver, [](std::string& result) {
        std::reverse(result.begin(), result.end());
    });
}

NativeResult stringSubstr(VM& vm, Value receiver, int, Value* args) 
{
    auto str = as<ObjString>(receiver);
    Value lo = args[0];
    Value hi = args[1];
    size_t length = str->str().length();
    auto rlo = normalIndex(lo, length);
    auto rhi = normalIndex(hi, length);

    if(rlo.result!=IndexResult::OK)
        return makeIndexError(lo, length, rlo.result);
    if(rhi.result!=IndexResult::OK && rhi.result!=IndexResult::END)
        return makeIndexError(hi, length, rhi.result);
    if(rlo.index > rhi.index)
        return NativeResult::failure(ErrorKind::VALUE_ERROR, "Substring lo is greater than hi.", Value());

    return NativeResult::success(Value(copyString(vm, str->str().substr(rlo.index, rhi.index-rlo.index))));
}

NativeResult stringFind(VM&, Value receiver, int, Value* args)
{
    auto str = as<ObjString>(receiver);
    
    if(!is<ObjString>(args[0]))
        return NativeResult::failure(ErrorKind::TYPE_ERROR, "String rfind() method expects a string as argument.", Value());

    size_t index = str->str().find(as<ObjString>(args[0])->str());
    return NativeResult::success(index>str->str().length()? Value(): Value(static_cast<double>(index)));
}

NativeResult stringRfind(VM&, Value receiver, int, Value* args)
{
    auto str = as<ObjString>(receiver);
    
    if(!is<ObjString>(args[0]))
        return NativeResult::failure(ErrorKind::TYPE_ERROR, "String .rfind() method expects a string as argument.", Value());

    size_t index = str->str().rfind(as<ObjString>(args[0])->str());
    return NativeResult::success(index>str->str().length()? Value(): Value(static_cast<double>(index)));
}

NativeResult stringStartsWith(VM&, Value receiver, int, Value* args)
{
    auto str = as<ObjString>(receiver);
    
    if(!is<ObjString>(args[0]))
        return NativeResult::failure(ErrorKind::TYPE_ERROR, "String .startswith() method expects a string as argument.", Value());

    bool b = str->str().starts_with(as<ObjString>(args[0])->str());
    return NativeResult::success(Value(b));
}

NativeResult stringEndsWith(VM&, Value receiver, int, Value* args)
{
    auto str = as<ObjString>(receiver);
    
    if(!is<ObjString>(args[0]))
        return NativeResult::failure(ErrorKind::TYPE_ERROR, "String .endswith() method expects a string as argument.", Value());

    bool b = str->str().ends_with(as<ObjString>(args[0])->str());
    return NativeResult::success(Value(b));
}

// -----

constexpr std::array properties {
    NativePropertyDef{"len", stringLength},
};

constexpr std::array methods {
    NativeMethodDef{"upper", 0, stringUpper},
    NativeMethodDef{"lower", 0, stringLower},
    NativeMethodDef{"trim", 0, stringTrim},
    NativeMethodDef{"reverse", 0, stringReverse},
    NativeMethodDef{"substr", 2, stringSubstr},
    NativeMethodDef{"find", 1, stringFind},
    NativeMethodDef{"rfind", 1, stringRfind},
    NativeMethodDef{"startswith", 1, stringStartsWith},
    NativeMethodDef{"endswith", 1, stringEndsWith},
};
}

std::span<const NativePropertyDef> stringNativeProperties() noexcept
{
    return properties;
}

std::span<const NativeMethodDef> stringNativeMethods() noexcept
{
    return methods;
}
