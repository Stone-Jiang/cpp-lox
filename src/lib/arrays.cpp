#include "arrays.h"

#include "../lang/object.h"
#include "../lang/vm.h"

#include <array>
#include <cmath>
#include <format>
#include <new>
#include <stdexcept>

namespace
{
NormalIndexInfo normalInsertIndex(const Value& value, size_t length)
{
    if(!value.is_number())
        return {0, IndexResult::NOT_NUMBER};

    const double raw = value.as_number();
    if(!std::isfinite(raw))
        return {0, IndexResult::NOT_FINITE};
    if(std::trunc(raw) != raw)
        return {0, IndexResult::NOT_INTEGRAL};
    if(std::abs(raw) > MAX_INDEX)
        return {0, IndexResult::NOT_SAFE};

    const i64 index = static_cast<i64>(raw);
    if(index >= 0)
    {
        const size_t position = static_cast<size_t>(index);
        if(position > length)
            return {0, IndexResult::OUT_OF_RANGE};
        return {position, IndexResult::OK};
    }

    const size_t distanceFromEnd = static_cast<size_t>(-index);
    if(distanceFromEnd > length)
        return {0, IndexResult::OUT_OF_RANGE};
    return {length - distanceFromEnd, IndexResult::OK};
}

NativeResult insertIndexError(
    Value index, size_t length, IndexResult reason)
{
    switch(reason)
    {
    case IndexResult::NOT_NUMBER:
        return NativeResult::failure(
            ErrorKind::INDEX_ERROR,
            "Array insertion index must be an integral number.",
            index);
    case IndexResult::NOT_FINITE:
        return NativeResult::failure(
            ErrorKind::INDEX_ERROR,
            "Array insertion index must be finite.",
            index);
    case IndexResult::NOT_INTEGRAL:
        return NativeResult::failure(
            ErrorKind::INDEX_ERROR,
            "Array insertion index must be integral.",
            index);
    case IndexResult::NOT_SAFE:
        return NativeResult::failure(
            ErrorKind::INDEX_ERROR,
            "Array insertion index is outside the integer range.",
            index);
    case IndexResult::OUT_OF_RANGE:
        return NativeResult::failure(
            ErrorKind::INDEX_ERROR,
            std::format(
                "Array insertion index is out of range for length {}.",
                length),
            index);
    case IndexResult::OK:
    default:
        return NativeResult::success(Value());
    }
}

NativeResult arrayLength(VM&, Value receiver)
{
    const auto* array = as<ObjArray>(receiver);
    return NativeResult::success(Value(static_cast<double>(array->len())));
}

NativeResult arrayPush(VM&, Value receiver, int, Value* args)
{
    auto array = as<ObjArray>(receiver);

    if(is<ObjArray>(args[0]) && as<ObjArray>(args[0]) == array)
    {
        return NativeResult::failure(
            ErrorKind::VALUE_ERROR,
            "An array can't be pushed into itself; push array.copy() instead.",
            args[0]);
    }

    try
    {
        array->elements.push_back(args[0]);
    }
    catch(const std::bad_alloc&)
    {
        return NativeResult::failure(
            ErrorKind::CRITICAL_ERROR,
            "Not enough memory to grow array.");
    }
    catch(const std::length_error&)
    {
        return NativeResult::failure(
            ErrorKind::CRITICAL_ERROR,
            "Array is too large to grow.");
    }

    return NativeResult::success(Value());
}

NativeResult arrayCopy(VM& vm, Value receiver, int, Value*)
{
    const auto* source = as<ObjArray>(receiver);
    auto* copy = makeObj<ObjArray>(vm);

    // The returned object is not on the caller's stack yet. Root it while
    // reserving and copying because either operation may trigger collection.
    vm.push(Value(copy));
    try
    {
        copy->elements.reserve(source->len());
        for(const Value& element: source->elements)
            copy->elements.push_back(element);
    }
    catch(const std::bad_alloc&)
    {
        vm.pop();
        return NativeResult::failure(
            ErrorKind::CRITICAL_ERROR,
            "Not enough memory to copy array.");
    }
    catch(const std::length_error&)
    {
        vm.pop();
        return NativeResult::failure(
            ErrorKind::CRITICAL_ERROR,
            "Array is too large to copy.");
    }
    vm.pop();

    return NativeResult::success(Value(copy));
}

NativeResult arrayPop(VM&, Value receiver, int, Value*)
{
    auto* array = as<ObjArray>(receiver);
    if(array->elements.empty())
    {
        return NativeResult::failure(
            ErrorKind::INDEX_ERROR,
            "Can't pop from an empty array.");
    }

    Value result = array->elements.back();
    array->elements.pop_back();
    return NativeResult::success(result);
}

NativeResult arrayInsert(VM&, Value receiver, int, Value* args)
{
    auto array = as<ObjArray>(receiver);
    Value raw = args[0];
    Value val = args[1];
    auto res = normalInsertIndex(raw, array->len());

    if(res.result!=IndexResult::OK)
        return insertIndexError(raw, array->len(), res.result);
    try
    {
        array->elements.insert(res.index, val);
    }
    catch(const std::bad_alloc&)
    {
        return NativeResult::failure(
            ErrorKind::CRITICAL_ERROR,
            "Not enough memory to grow array.");
    }
    catch(const std::length_error&)
    {
        return NativeResult::failure(
            ErrorKind::CRITICAL_ERROR,
            "Array is too large to grow.");
    }
    
    return NativeResult::success(Value());
}

NativeResult arrayClear(VM&, Value receiver, int, Value*)
{
    auto array = as<ObjArray>(receiver);
    
    array->elements.clear();
    return NativeResult::success(Value());
}

NativeResult arrayReverse(VM&, Value receiver, int, Value*)
{
    auto array = as<ObjArray>(receiver);
    
    std::reverse(array->elements.begin(), array->elements.end());
    return NativeResult::success(Value());
}

// -----

constexpr std::array properties {
    NativePropertyDef{"len", arrayLength},
};

constexpr std::array methods {
    NativeMethodDef{"push", 1, arrayPush},
    NativeMethodDef{"pop", 0, arrayPop},
    NativeMethodDef{"insert", 2, arrayInsert},
    NativeMethodDef{"clear", 0, arrayClear},
    NativeMethodDef{"copy", 0, arrayCopy},
    NativeMethodDef{"reverse", 0, arrayReverse},
};
}

std::span<const NativePropertyDef> arrayNativeProperties() noexcept
{
    return properties;
}

std::span<const NativeMethodDef> arrayNativeMethods() noexcept
{
    return methods;
}
