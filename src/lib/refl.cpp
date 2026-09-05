#include "refl.h"
#include "../lang/vm.h"
#include "../lang/object.h"
#include "../lang/table.h"
#include <array>

namespace
{
bool findClassMember(ObjClass* klass, ObjString* name, ClassMember& member)
{
    for(ObjClass* cur = klass; cur != nullptr; cur = cur->superclass)
    {
        if(cur->members.get(name, member))
            return true;
    }
    return false;
}


NativeResult insClass(VM&, Value receiver)
{
    const auto ins = as<ObjInstance>(receiver);
    return NativeResult::success(Value(ins->klass));
}

NativeResult isInstance(VM&, Value receiver, int, Value* args)
{
    if(!is<ObjClass>(args[0]))
        return NativeResult::failure(ErrorKind::TYPE_ERROR, "is_ins() expects a class.");
    
    auto ins = as<ObjInstance>(receiver);
    auto cls = ins->klass;
    auto target = as<ObjClass>(args[0]);

    while(cls != nullptr) 
    {
        if(cls==target)
            return NativeResult::success(Value(true));
        cls = cls->superclass;
    }
    return NativeResult::success(Value(false));
}

NativeResult insFields(VM& vm, Value receiver, int, Value*)
{
    auto ins = as<ObjInstance>(receiver);
    const auto& map = ins->fields;

    auto dest = makeObj<ObjMap>(vm);
    vm.push(Value(dest));

    try
    {
        dest->vt.reserve(map.size());
        map.foreach([dest](ObjString* key, Value val){
            dest->vt.set(Value(key), val);
        });
    }
    catch(const std::bad_alloc&)
    {
        vm.pop();
        return NativeResult::failure(ErrorKind::CRITICAL_ERROR, "Not enough memory to convert object to a map.");
    }
    catch(const std::length_error&)
    {
        vm.pop();
        return NativeResult::failure(ErrorKind::CRITICAL_ERROR, "Object too large to convert to a map.");
    }
    catch(const std::invalid_argument& error)
    {
        vm.pop();
        return NativeResult::failure(ErrorKind::TYPE_ERROR, error.what());
    }

    vm.pop();
    return NativeResult::success(Value(dest));
}

NativeResult insFieldnames(VM& vm, Value receiver, int, Value*)
{
    auto ins = as<ObjInstance>(receiver);
    const auto& map = ins->fields;

    auto dest = makeObj<ObjArray>(vm);
    vm.push(Value(dest));

    try
    {
        dest->elements.reserve(map.size());
        map.foreach([dest](ObjString* key, Value){
            dest->elements.push_back(Value(key));
        });
    }
    catch(const std::bad_alloc&)
    {
        vm.pop();
        return NativeResult::failure(ErrorKind::CRITICAL_ERROR, "Not enough memory to convert object to an array.");
    }
    catch(const std::length_error&)
    {
        vm.pop();
        return NativeResult::failure(ErrorKind::CRITICAL_ERROR, "Object too large to convert to an array.");
    }
    catch(const std::invalid_argument& error)
    {
        vm.pop();
        return NativeResult::failure(ErrorKind::TYPE_ERROR, error.what());
    }

    vm.pop();
    return NativeResult::success(Value(dest));
}

NativeResult insHasField(VM&, Value receiver, int, Value* args)
{
    if(!is<ObjString>(args[0]))
        return NativeResult::failure(ErrorKind::TYPE_ERROR, "has_field() expects a string name.");

    auto ins = as<ObjInstance>(receiver);
    Value value;
    if(ins->fields.get(as<ObjString>(args[0]), value))
        return NativeResult::success(Value(true));
    else
        return NativeResult::success(Value(false));
}

NativeResult insGetField(VM&, Value receiver, int, Value* args)
{
    if(!is<ObjString>(args[0]))
        return NativeResult::failure(ErrorKind::TYPE_ERROR, "get_field() expects a string name.");

    auto ins = as<ObjInstance>(receiver);
    Value value;
    if(ins->fields.get(as<ObjString>(args[0]), value))
        return NativeResult::success(value);
    return NativeResult::failure(ErrorKind::NAME_ERROR, std::format("get_field: '{}' has no field named '{}'.", to_string(ins), to_string(args[0])), receiver);
}

NativeResult insSetField(VM&, Value receiver, int, Value* args)
{
    if(!is<ObjString>(args[0]))
        return NativeResult::failure(ErrorKind::TYPE_ERROR, "set_field() expects a string name.", Value());

    auto ins = as<ObjInstance>(receiver);
    if(ins->fields.set(as<ObjString>(args[0]), args[1]))
        return NativeResult::success(Value(true));
    return NativeResult::success(Value(false));
}

NativeResult insRemoveField(VM&, Value receiver, int, Value* args)
{
    if(!is<ObjString>(args[0]))
        return NativeResult::failure(ErrorKind::TYPE_ERROR, "rmv_field() expects a string name.");

    auto ins = as<ObjInstance>(receiver);
    if(ins->fields.del(as<ObjString>(args[0])))
        return NativeResult::success(Value());
    return NativeResult::failure(ErrorKind::NAME_ERROR, std::format("rmv_field: '{}' has no field named '{}'.", to_string(ins), to_string(args[0])), receiver);
}

NativeResult insHasMethod(VM&, Value receiver, int, Value* args)
{
    if(!is<ObjString>(args[0]))
        return NativeResult::failure(ErrorKind::TYPE_ERROR, "has_method() expects a string name.");

    auto cls = as<ObjInstance>(receiver)->klass;
    ClassMember value;
    if(findClassMember(cls, as<ObjString>(args[0]), value) && !value.isStatic)
        return NativeResult::success(Value(true));
    else
        return NativeResult::success(Value(false));
}

NativeResult insGetMethod(VM& vm, Value receiver, int, Value* args)
{
    if(!is<ObjString>(args[0]))
        return NativeResult::failure(ErrorKind::TYPE_ERROR, "get_method() expects a string name.");

    auto ins = as<ObjInstance>(receiver);
    auto name = as<ObjString>(args[0]);

    ClassMember member;
    if(!findClassMember(ins->klass, name, member))
    {
        return NativeResult::failure(ErrorKind::NAME_ERROR, std::format("get_method: '{}' has no method named '{}'.", to_string(ins), name->str()), receiver);
    }

    if(member.isStatic)
    {
        return NativeResult::failure(ErrorKind::NAME_ERROR, std::format("'{}' is static. Cannot bind to an instance.", name->str()), receiver);
    }

    if(!is<ObjClosure>(member.value))
    {
        return NativeResult::failure(ErrorKind::TYPE_ERROR,std::format("'{}' is not an instance method.", name->str()), receiver);
    }

    auto bound = makeObj<ObjBoundMethod>(vm, receiver, as<ObjClosure>(member.value));
    return NativeResult::success(Value(bound));
}

NativeResult insGet(VM& vm, Value receiver, int, Value* args)
{
    if(!is<ObjString>(args[0]))
        return NativeResult::failure(ErrorKind::TYPE_ERROR, "get_method() expects a string name.");

    auto ins = as<ObjInstance>(receiver);

    auto field = insGetField(vm, receiver, 1, args);
    auto method = insGetMethod(vm, receiver, 1, args);
    if(field.ok)
        return field;
    else if(method.ok)
        return method;
    return NativeResult::failure(ErrorKind::NAME_ERROR, std::format("get: '{}' has no field/method named '{}'.", to_string(ins), as<ObjString>(args[0])->str()), receiver);
}

// -----

NativeResult clsName(VM&, Value receiver)
{
    const auto cls = as<ObjClass>(receiver);
    return NativeResult::success(Value(cls->name));
}

NativeResult clsSuperclass(VM&, Value receiver)
{
    auto cls = as<ObjClass>(receiver);
    return NativeResult::success(cls->superclass==nullptr? Value(): Value(cls->superclass));
}

NativeResult isSubclass(VM&, Value receiver, int, Value* args)
{
    if(!is<ObjClass>(args[0]))
        return NativeResult::failure(ErrorKind::TYPE_ERROR, "is_ins() expects a class.", Value());
    
    auto cls = as<ObjClass>(receiver);
    auto target = as<ObjClass>(args[0]);

    while(cls != nullptr) 
    {
        if(cls==target)
            return NativeResult::success(Value(true));
        cls = cls->superclass;
    }
    return NativeResult::success(Value(false));
}

NativeResult isSuperclass(VM&, Value receiver, int, Value* args)
{
    if(!is<ObjClass>(args[0]))
        return NativeResult::failure(ErrorKind::TYPE_ERROR, "is_ins() expects a class.", Value());
    
    auto cls = as<ObjClass>(receiver);
    auto target = as<ObjClass>(args[0]);

    while(target != nullptr) 
    {
        if(cls==target)
            return NativeResult::success(Value(true));
        target = target->superclass;
    }
    return NativeResult::success(Value(false));
}

NativeResult clsSuperclasses(VM& vm, Value receiver, int, Value*)
{
    auto cls = as<ObjClass>(receiver);
    auto arr = makeObj<ObjArray>(vm);
    vm.push(Value(arr));

    while(cls != nullptr)
    {
        arr->elements.push_back(cls);
        cls = cls->superclass;
    }

    vm.pop();
    return NativeResult::success(Value(arr));
}

NativeResult clsHasMethod(VM&, Value receiver, int, Value* args)
{
    if(!is<ObjString>(args[0]))
        return NativeResult::failure(ErrorKind::TYPE_ERROR, "has_method() expects a string name.", Value());

    auto cls = as<ObjClass>(receiver);
    ClassMember value;
    if(findClassMember(cls, as<ObjString>(args[0]), value) && !value.isStatic)
        return NativeResult::success(Value(true));
    else
        return NativeResult::success(Value(false));
}

NativeResult clsHasStatic(VM&, Value receiver, int, Value* args)
{
    if(!is<ObjString>(args[0]))
        return NativeResult::failure(ErrorKind::TYPE_ERROR, "has_static() expects a string name.", Value());

    auto cls = as<ObjClass>(receiver);
    ClassMember value;
    if(findClassMember(cls, as<ObjString>(args[0]), value) && value.isStatic)
        return NativeResult::success(Value(true));
    else
        return NativeResult::success(Value(false));
}

NativeResult clsGetStatic(VM&, Value receiver, int, Value* args)
{
    if(!is<ObjString>(args[0]))
        return NativeResult::failure(ErrorKind::TYPE_ERROR,"get_static() expects a string name.");

    auto cls = as<ObjClass>(receiver);
    auto name = as<ObjString>(args[0]);
    ClassMember member;

    if(!findClassMember(cls, name, member))
    {
        return NativeResult::failure(ErrorKind::NAME_ERROR, std::format("get_static: '{}' has no static method named '{}'.", to_string(cls), name->str()), receiver);
    }

    if(!member.isStatic)
    {
        return NativeResult::failure(ErrorKind::TYPE_ERROR, std::format("'{}' is an instance method, not a static method.", name->str()), receiver);
    }

    if(!is<ObjClosure>(member.value) && !is<ObjNative>(member.value) && !is<ObjBoundMethod>(member.value))
    {
        return NativeResult::failure(ErrorKind::TYPE_ERROR,std::format("Static member '{}' is not callable.", name->str()),receiver);
    }

    // Static methods do not receive a class or instance receiver. Their
    // closure already retains any lexical upvalues it needs.
    return NativeResult::success(member.value);
}

NativeResult clsAllMethodNames(VM& vm, Value receiver, int, Value*)
{
    auto cls = as<ObjClass>(receiver); 
    const auto& table = cls->members;
    auto dest = makeObj<ObjMap>(vm);
    vm.push(Value(dest));

    try
    {
        dest->vt.reserve(table.size());
        table.foreach([dest](ObjString* key, ClassMember val){
            dest->vt.set(Value(key), Value(val.isStatic));
        });
    }
    catch(const std::bad_alloc&)
    {
        vm.pop();
        return NativeResult::failure(ErrorKind::CRITICAL_ERROR, "Not enough memory to write methods & statics names to array.", Value());
    }
    catch(const std::length_error&)
    {
        vm.pop();
        return NativeResult::failure(ErrorKind::CRITICAL_ERROR, "Too many methods & statics to write to array.", Value());
    }
    catch(const std::invalid_argument& error)
    {
        vm.pop();
        return NativeResult::failure(ErrorKind::TYPE_ERROR, error.what(), Value());
    }

    vm.pop();
    return NativeResult::success(Value(dest));
}


// -----

constexpr std::array instanceProperties {
    NativePropertyDef{"cls", insClass},
};

constexpr std::array instanceMethods {
    NativeMethodDef{"is_ins", 1, isInstance},
    NativeMethodDef{"fields", 0, insFields},
    NativeMethodDef{"field_names", 0, insFieldnames},
    NativeMethodDef{"has_field", 1, insHasField},
    NativeMethodDef{"get_field", 1, insGetField},
    NativeMethodDef{"set_field", 2, insSetField},
    NativeMethodDef{"rmv_field", 1, insRemoveField},
    NativeMethodDef{"has_method", 1, insHasMethod},
    NativeMethodDef{"get_method", 1, insGetMethod},
    NativeMethodDef{"get", 1, insGet},
};

constexpr std::array classProperties {
    NativePropertyDef{"name", clsName},
    NativePropertyDef{"superclass", clsSuperclass},
};

constexpr std::array classMethods {
    NativeMethodDef{"is_sub", 1, isSubclass},
    NativeMethodDef{"is_sup", 1, isSuperclass},
    NativeMethodDef{"superclasses", 0, clsSuperclasses},
    NativeMethodDef{"has_method", 1, clsHasMethod},
    NativeMethodDef{"has_static", 1, clsHasStatic},
    NativeMethodDef{"get_static", 1, clsGetStatic},
    NativeMethodDef{"all_method_names", 0, clsAllMethodNames},

};

}


std::span<const NativePropertyDef> instanceNativeProperties() noexcept
{
    return instanceProperties;
}

std::span<const NativeMethodDef> instanceNativeMethods() noexcept
{
    return instanceMethods;
}

std::span<const NativePropertyDef> classNativeProperties() noexcept
{
    return classProperties;
}

std::span<const NativeMethodDef> classNativeMethods() noexcept
{
    return classMethods;
}
