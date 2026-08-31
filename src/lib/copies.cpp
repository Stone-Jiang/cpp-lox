#include "copies.h"

#include "../lang/object.h"
#include "../lang/vm.h"

#include <new>
#include <stdexcept>
#include <unordered_map>

namespace
{
class DeepCopyContext
{
    VM& vm;
    ObjArray* roots;
    std::unordered_map<const Obj*, Obj*> copies;

    template<Objective T>
    T* remember(const Obj* source, T* copy)
    {
        // Root the new object before either the root vector or memo table can
        // allocate. The root array keeps it alive after this temporary stack
        // slot is removed.
        vm.push(Value(copy));
        try
        {
            roots->elements.push_back(Value(copy));
            copies.emplace(source, copy);
        }
        catch(...)
        {
            vm.pop();
            throw;
        }
        vm.pop();
        return copy;
    }

    Value copyArray(const ObjArray* source)
    {
        if(const auto found = copies.find(source); found != copies.end())
            return Value(found->second);

        auto* result = remember(source, makeObj<ObjArray>(vm));
        result->elements.reserve(source->len());
        for(const Value& element: source->elements)
            result->elements.push_back(copy(element));
        return Value(result);
    }

    Value copyMap(const ObjMap* source)
    {
        if(const auto found = copies.find(source); found != copies.end())
            return Value(found->second);

        auto* result = remember(source, makeObj<ObjMap>(vm));
        result->vt.reserve(source->len());
        source->vt.foreach([&](const Value& key, const Value& value) {
            Value copiedKey = copy(key);
            Value copiedValue = copy(value);
            result->vt.set(copiedKey, copiedValue);
        });
        return Value(result);
    }

public:
    DeepCopyContext(VM& vm, ObjArray* roots): vm(vm), roots(roots) {}

    Value copy(Value source)
    {
        if(is<ObjArray>(source))
            return copyArray(as<ObjArray>(source));
        if(is<ObjMap>(source))
            return copyMap(as<ObjMap>(source));

        // Strings and all other object types retain their ordinary reference
        // semantics. Only collection storage is recursively duplicated.
        return source;
    }
};
}

NativeResult deepCopyCollection(VM& vm, Value source)
{
    auto* roots = makeObj<ObjArray>(vm);
    vm.push(Value(roots));

    try
    {
        DeepCopyContext context(vm, roots);
        Value result = context.copy(source);
        vm.pop();
        return NativeResult::success(result);
    }
    catch(const std::bad_alloc&)
    {
        vm.pop();
        return NativeResult::failure(
            ErrorKind::CRITICAL_ERROR,
            "Not enough memory to deep-copy collection.",
            source);
    }
    catch(const std::length_error&)
    {
        vm.pop();
        return NativeResult::failure(
            ErrorKind::CRITICAL_ERROR,
            "Collection is too large to deep-copy.",
            source);
    }
    catch(const std::invalid_argument& error)
    {
        vm.pop();
        return NativeResult::failure(
            ErrorKind::TYPE_ERROR,
            error.what(),
            source);
    }
}
