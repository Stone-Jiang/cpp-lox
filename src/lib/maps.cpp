#include "maps.h"
#include "copies.h"
#include "../lang/object.h"
#include "../lang/vm.h"
#include <array>

namespace
{
NativeResult mapConstructor(VM& vm, int, Value*)
{
    return NativeResult::success(Value(makeObj<ObjMap>(vm)));
}

NativeResult mapLength(VM&, Value receiver)
{
    const auto* map = as<ObjMap>(receiver);
    return NativeResult::success(
        Value(static_cast<double>(map->len())));
}

NativeResult mapClear(VM&, Value receiver, int, Value*)
{
    as<ObjMap>(receiver)->vt.clear();
    return NativeResult::success(Value());
}

NativeResult mapCopy(VM& vm, Value receiver, int, Value*)
{
    const auto source = as<ObjMap>(receiver);
    auto copy = makeObj<ObjMap>(vm);
    vm.push(Value(copy));

    try
    {
        copy->vt.reserve(source->len());
        source->vt.foreach([copy](const Value& key, const Value& value) {
            copy->vt.set(key, value);
        });
    }
    catch(const std::bad_alloc&)
    {
        vm.pop();
        return NativeResult::failure(ErrorKind::CRITICAL_ERROR, "Not enough memory to copy map.", receiver);
    }
    catch(const std::length_error&)
    {
        vm.pop();
        return NativeResult::failure(ErrorKind::CRITICAL_ERROR, "Map is too large to copy.", receiver);
    }
    catch(const std::invalid_argument& error)
    {
        vm.pop();
        return NativeResult::failure(ErrorKind::TYPE_ERROR, error.what(), receiver);
    }

    vm.pop();
    return NativeResult::success(Value(copy));
}

NativeResult mapDeepCopy(VM& vm, Value receiver, int, Value*)
{
    return deepCopyCollection(vm, receiver);
}

NativeResult mapRemove(VM&, Value receiver, int, Value* args)
{
    auto map = as<ObjMap>(receiver);
    Value result;
    if(map->vt.remove(args[0], result))
        return NativeResult::success(result);
    return NativeResult::success(Value());
}

NativeResult mapGetDefault(VM&, Value receiver, int, Value* args)
{
    auto map = as<ObjMap>(receiver);
    Value result;
    if(map->vt.get(args[0], result))
        return NativeResult::success(result);
    return NativeResult::success(args[1]);
}

NativeResult mapRemoveDefault(VM&, Value receiver, int, Value* args)
{
    auto map = as<ObjMap>(receiver);
    Value result;
    if(map->vt.remove(args[0], result))
        return NativeResult::success(result);
    return NativeResult::success(args[1]);
}

NativeResult mapGetSet(VM&, Value receiver, int, Value* args)
{
    auto map = as<ObjMap>(receiver);
    Value result;
    if(map->vt.get(args[0], result))
        return NativeResult::success(result);
    map->vt.set(args[0], args[1]);
    return NativeResult::success(args[1]);
}

NativeResult mapContains(VM&, Value receiver, int, Value* args)
{
    auto map = as<ObjMap>(receiver);
    return NativeResult::success(Value(map->vt.contains(args[0])));
}

NativeResult mapKeys(VM& vm, Value receiver, int, Value*)
{
    auto map = as<ObjMap>(receiver);
    auto array = makeObj<ObjArray>(vm);
    vm.push(Value(array));

    try 
    {
        array->elements.reserve(map->len());
        map->vt.foreach([array](const Value& key, const Value&) {
            array->elements.push_back(key);
        });
    }
    catch(const std::bad_alloc&)
    {
        vm.pop();
        return NativeResult::failure(ErrorKind::CRITICAL_ERROR, "Not enough memory to convert to array.");
    }
    catch(const std::length_error&)
    {
        vm.pop();
        return NativeResult::failure(ErrorKind::CRITICAL_ERROR, "Map is too large to convert into array.");
    }

    vm.pop();
    return NativeResult::success(Value(array));
}

NativeResult mapValues(VM& vm, Value receiver, int, Value*)
{
    auto map = as<ObjMap>(receiver);
    auto array = makeObj<ObjArray>(vm);
    vm.push(Value(array));

    try 
    {
        array->elements.reserve(map->len());
        map->vt.foreach([array](const Value&, const Value& value) {
            array->elements.push_back(value);
        });
    }
    catch(const std::bad_alloc&)
    {
        vm.pop();
        return NativeResult::failure(ErrorKind::CRITICAL_ERROR, "Not enough memory to convert to array.");
    }
    catch(const std::length_error&)
    {
        vm.pop();
        return NativeResult::failure(ErrorKind::CRITICAL_ERROR, "Map is too large to convert into array.");
    }

    vm.pop();
    return NativeResult::success(Value(array));
}

NativeResult mapItems(VM& vm, Value receiver, int, Value*)
{
    auto map = as<ObjMap>(receiver);
    auto array = makeObj<ObjArray>(vm);
    vm.push(Value(array));

    try 
    {
        array->elements.reserve(map->len());
        map->vt.foreach([&vm, array](const Value& key, const Value& value) {
            auto pair = makeObj<ObjArray>(vm);
            vm.push(Value(pair));
            try
            {
                pair->elements.reserve(2);
                pair->elements.push_back(key);
                pair->elements.push_back(value);
                array->elements.push_back(Value(pair));
            }
            catch(...)
            {
                vm.pop();
                throw;
            }
            vm.pop();
        });
    }
    catch(const std::bad_alloc&)
    {
        vm.pop();
        return NativeResult::failure(ErrorKind::CRITICAL_ERROR, "Not enough memory to convert to array.");
    }
    catch(const std::length_error&)
    {
        vm.pop();
        return NativeResult::failure(ErrorKind::CRITICAL_ERROR, "Map is too large to convert into array.");
    }

    vm.pop();
    return NativeResult::success(Value(array));
}

NativeResult mapUpdate(VM&, Value receiver, int, Value* args)
{
    if(!is<ObjMap>(args[0]))
        return NativeResult::failure(ErrorKind::TYPE_ERROR, "Map update() expects another map.");

    auto to = as<ObjMap>(receiver);
    auto from = as<ObjMap>(args[0]);

    try
    {
        to->vt.reserve(to->len() + from->len());
        from->vt.foreach([to](const Value& key, const Value& val) {
            to->vt.set(key, val);
        });
    }
    catch(const std::bad_alloc&)
    {
        return NativeResult::failure(
            ErrorKind::CRITICAL_ERROR,
            "Not enough memory to update map.",
            receiver);
    }
    catch(const std::length_error&)
    {
        return NativeResult::failure(
            ErrorKind::CRITICAL_ERROR,
            "Map is too large to update.",
            receiver);
    }
    catch(const std::invalid_argument& error)
    {
        return NativeResult::failure(
            ErrorKind::TYPE_ERROR,
            error.what(),
            receiver);
    }

    return NativeResult::success(Value());
}

NativeResult mapMerge(VM& vm, Value receiver, int, Value* args)
{
    if(!is<ObjMap>(args[0]))
        return NativeResult::failure(ErrorKind::TYPE_ERROR, "Map merge() expects another map.");

    const auto source = as<ObjMap>(receiver);
    const auto other = as<ObjMap>(args[0]);
    auto copy = makeObj<ObjMap>(vm);
    vm.push(Value(copy));

    try
    {
        copy->vt.reserve(source->len() + other->len());
        source->vt.foreach([copy](const Value& key, const Value& value) {
            copy->vt.set(key, value);
        });
        other->vt.foreach([copy](const Value& key, const Value& value) {
            copy->vt.set(key, value);
        });
    }
    catch(const std::bad_alloc&)
    {
        vm.pop();
        return NativeResult::failure(ErrorKind::CRITICAL_ERROR, "Not enough memory to merge map.", receiver);
    }
    catch(const std::length_error&)
    {
        vm.pop();
        return NativeResult::failure(ErrorKind::CRITICAL_ERROR, "Map is too large to merge.", receiver);
    }
    catch(const std::invalid_argument& error)
    {
        vm.pop();
        return NativeResult::failure(ErrorKind::TYPE_ERROR, error.what(), receiver);
    }

    vm.pop();
    return NativeResult::success(Value(copy));
}

NativeResult mapInvert(VM& vm, Value receiver, int, Value*)
{
    const auto source = as<ObjMap>(receiver);
    auto copy = makeObj<ObjMap>(vm);
    vm.push(Value(copy));

    try
    {
        copy->vt.reserve(source->len());
        source->vt.foreach([copy](const Value& key, const Value& value) {
            copy->vt.set(value, key);
        });
    }
    catch(const std::bad_alloc&)
    {
        vm.pop();
        return NativeResult::failure(ErrorKind::CRITICAL_ERROR, "Not enough memory to copy map.", receiver);
    }
    catch(const std::length_error&)
    {
        vm.pop();
        return NativeResult::failure(ErrorKind::CRITICAL_ERROR, "Map is too large to copy.", receiver);
    }
    catch(const std::invalid_argument& error)
    {
        vm.pop();
        return NativeResult::failure(ErrorKind::TYPE_ERROR, error.what(), receiver);
    }

    vm.pop();
    return NativeResult::success(Value(copy));
}

constexpr std::array definitions {
    NativeDef{"Map", 0, mapConstructor},
};

constexpr std::array properties {
    NativePropertyDef{"len", mapLength},
};

constexpr std::array methods {
    NativeMethodDef{"clear", 0, mapClear},
    NativeMethodDef{"copy", 0, mapCopy},
    NativeMethodDef{"deep_copy", 0, mapDeepCopy},
    NativeMethodDef{"has_key", 1, mapContains},
    NativeMethodDef{"remove", 1, mapRemove},
    NativeMethodDef{"get_or", 2, mapGetDefault},
    NativeMethodDef{"remove_or", 2, mapRemoveDefault},
    NativeMethodDef{"get_or_set", 2, mapGetSet},
    NativeMethodDef{"keys", 0, mapKeys},
    NativeMethodDef{"values", 0, mapValues},
    NativeMethodDef{"items", 0, mapItems},
    NativeMethodDef{"update", 1, mapUpdate},
    NativeMethodDef{"merge", 1, mapMerge},
    NativeMethodDef{"invert", 0, mapInvert},
};
}

std::span<const NativeDef> mapNativeDefinitions() noexcept
{
    return definitions;
}

std::span<const NativePropertyDef> mapNativeProperties() noexcept
{
    return properties;
}

std::span<const NativeMethodDef> mapNativeMethods() noexcept
{
    return methods;
}
