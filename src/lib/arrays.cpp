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
NativeResult makeIndexError(Value index, size_t length, IndexResult reason)
{
    switch(reason)
    {
    case IndexResult::NOT_NUMBER:
        return NativeResult::failure(ErrorKind::INDEX_ERROR, "Array index must be an integral number.", index);
    case IndexResult::NOT_FINITE:
        return NativeResult::failure(ErrorKind::INDEX_ERROR, "Array index must be finite.", index);
    case IndexResult::NOT_INTEGRAL:
        return NativeResult::failure(ErrorKind::INDEX_ERROR, "Array index must be integral.", index);
    case IndexResult::NOT_SAFE:
        return NativeResult::failure(ErrorKind::INDEX_ERROR, "Array index is outside the integer range.", index);
    case IndexResult::OUT_OF_RANGE:
    case IndexResult::END:
        return NativeResult::failure(ErrorKind::INDEX_ERROR, std::format("Array index is out of range for length {}.", length), index);
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
        return NativeResult::failure(ErrorKind::VALUE_ERROR, "An array can't be pushed into itself; push array.copy() instead.", args[0]);
    }

    try
    {
        array->elements.push_back(args[0]);
    }
    catch(const std::bad_alloc&)
    {
        return NativeResult::failure(ErrorKind::CRITICAL_ERROR, "Not enough memory to grow array.");
    }
    catch(const std::length_error&)
    {
        return NativeResult::failure(ErrorKind::CRITICAL_ERROR, "Array is too large to grow.");
    }

    return NativeResult::success(Value());
}

NativeResult arrayCopy(VM& vm, Value receiver, int, Value*)
{
    const auto source = as<ObjArray>(receiver);
    auto copy = makeObj<ObjArray>(vm);

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
        return NativeResult::failure(ErrorKind::CRITICAL_ERROR, "Not enough memory to copy array.");
    }
    catch(const std::length_error&)
    {
        vm.pop();
        return NativeResult::failure(ErrorKind::CRITICAL_ERROR, "Array is too large to copy.");
    }
    vm.pop();

    return NativeResult::success(Value(copy));
}

NativeResult arrayPop(VM&, Value receiver, int, Value*)
{
    auto array = as<ObjArray>(receiver);
    if(array->elements.empty())
    {
        return NativeResult::failure(ErrorKind::INDEX_ERROR, "Can't pop from an empty array.");
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
    auto res = normalIndex(raw, array->len());

    if(res.result!=IndexResult::OK && res.result!=IndexResult::END)
        return makeIndexError(raw, array->len(), res.result);
    try
    {
        array->elements.insert(res.index, val);
    }
    catch(const std::bad_alloc&)
    {
        return NativeResult::failure(ErrorKind::CRITICAL_ERROR, "Not enough memory to grow array.");
    }
    catch(const std::length_error&)
    {
        return NativeResult::failure(ErrorKind::CRITICAL_ERROR, "Array is too large to grow.");
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

NativeResult arrayConcat(VM&, Value receiver, int, Value* args)
{
    auto array = as<ObjArray>(receiver);
    
    if(!is<ObjArray>(args[0]))
    {
        return NativeResult::failure(ErrorKind::TYPE_ERROR, "Array .concat() requires another array.");
    }
    
    auto b = as<ObjArray>(args[0]);
    
    try
    {
        array->elements.reserve(array->len() + b->len());
        for(const auto& val: b->elements)
            array->elements.push_back(val);
    }
    catch(const std::bad_alloc&)
    {
        return NativeResult::failure(ErrorKind::CRITICAL_ERROR, "Not enough memory to grow array.");
    }
    catch(const std::length_error&)
    {
        return NativeResult::failure(ErrorKind::CRITICAL_ERROR, "Array is too large to grow.");
    }

    return NativeResult::success(Value());
}

NativeResult arrayErase(VM&, Value receiver, int, Value* args)
{
    auto array = as<ObjArray>(receiver);
    Value raw = args[0];
    auto res = normalIndex(raw, array->len());

    if(res.result!=IndexResult::OK)
        return makeIndexError(raw, array->len(), res.result);

    array->elements.erase(res.index);
    return NativeResult::success(Value());
}

NativeResult arrayRemove(VM&, Value receiver, int, Value* args)
{
    auto array = as<ObjArray>(receiver);
    for(size_t i = 0; i<array->len(); i++)
    {
        if(array->elements[i]==args[0])
        {
            array->elements.erase(i);
            return NativeResult::success(Value(true));
        }
    }
    return NativeResult::success(Value(false));
}

NativeResult arrayFront(VM&, Value receiver, int, Value*)
{
    auto array = as<ObjArray>(receiver);
    if(array->elements.empty())
    {
        return NativeResult::failure(ErrorKind::INDEX_ERROR, "Can't pop from an empty array.");
    }
    return NativeResult::success(Value(array->elements.front()));
}

NativeResult arrayBack(VM&, Value receiver, int, Value*)
{
    auto array = as<ObjArray>(receiver);
    if(array->elements.empty())
    {
        return NativeResult::failure(ErrorKind::INDEX_ERROR, "Can't pop from an empty array.");
    }
    return NativeResult::success(Value(array->elements.back()));
}

NativeResult arrayCount(VM&, Value receiver, int, Value* args)
{
    auto array = as<ObjArray>(receiver);
    size_t count = 0;
    for(size_t i = 0; i<array->len(); i++)
        if(array->elements[i]==args[0])
            count += 1;
        
    return NativeResult::success(Value(static_cast<double>(count)));
}

NativeResult arrayFind(VM&, Value receiver, int, Value* args)
{
    auto array = as<ObjArray>(receiver);

    for(size_t i = 0; i<array->len(); i++)
        if(array->elements[i]==args[0])
            return NativeResult::success(Value(static_cast<double>(i)));
        
    return NativeResult::success(Value(-1.0));
}

NativeResult arraySlice(VM& vm, Value receiver, int, Value* args)
{
    auto array = as<ObjArray>(receiver);
    Value lo = args[0];
    Value hi = args[1];
    auto rlo = normalIndex(lo, array->len());
    auto rhi = normalIndex(hi, array->len());

    if(rlo.result!=IndexResult::OK)
        return makeIndexError(lo, array->len(), rlo.result);
    if(rhi.result!=IndexResult::OK && rhi.result!=IndexResult::END)
        return makeIndexError(hi, array->len(), rhi.result);
    

    if(rlo.index > rhi.index)
        return NativeResult::failure(ErrorKind::VALUE_ERROR, "Slice lo is greater than hi.", Value());

    auto slice = makeObj<ObjArray>(vm);

    vm.push(Value(slice));
    try
    {
        slice->elements.reserve(rhi.index - rlo.index);
        for(size_t i=rlo.index; i<rhi.index; i++)
            slice->elements.push_back(array->elements[i]);
    }
    catch(const std::bad_alloc&)
    {
        vm.pop();
        return NativeResult::failure(ErrorKind::CRITICAL_ERROR, "Not enough memory to make slice.");
    }
    catch(const std::length_error&)
    {
        vm.pop();
        return NativeResult::failure(ErrorKind::CRITICAL_ERROR, "Slice is too large to make.");
    }
    vm.pop();

    return NativeResult::success(Value(slice));
}

NativeResult arrayJoin(VM& vm, Value receiver, int, Value* args)
{
    auto sep = args[0];
    if(!sep.is_obj() || !is<ObjString>(sep))
        return NativeResult::failure(ErrorKind::TYPE_ERROR, "Separator must be a string.", Value());

    auto array = as<ObjArray>(receiver);

    std::string s;
    s.reserve(array->len()*2);
    for(size_t i=0; i<array->len()-1; i++)
    {
        s += to_string(array->elements[i]);
        s += as<ObjString>(sep)->str();
    }
    s += to_string(array->elements.back());
    return NativeResult::success(copyString(vm, s));
}

NativeResult arraySort(VM&, Value receiver, int, Value*)
{
    auto array = as<ObjArray>(receiver);
    std::sort(array->elements.begin(), array->elements.end());    
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
    NativeMethodDef{"concat", 1, arrayConcat},
    NativeMethodDef{"erase", 1, arrayErase},
    NativeMethodDef{"remove", 1, arrayRemove},
    NativeMethodDef{"front", 0, arrayFront},
    NativeMethodDef{"back", 0, arrayBack},
    NativeMethodDef{"count", 1, arrayCount},
    NativeMethodDef{"slice", 2, arraySlice},
    NativeMethodDef{"join", 1, arrayJoin},
    NativeMethodDef{"find", 1, arrayFind},
    NativeMethodDef{"sort", 0, arraySort},
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
