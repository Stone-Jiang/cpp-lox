#include "object.h"

std::optional<ObjType> objType(const Value& value)
{
    if(value.is_obj())
        return value.as_obj()->type;
    return std::nullopt;
}

bool isType(const Value& value, ObjType type)
{
    return value.is_obj() && value.as_obj()->type == type;
}

bool is_str(const Value& value)
{
    return isType(value, ObjType::STRING);
}

const ObjString* as_str(const Value& value)
{
    return static_cast<const ObjString*>(value.as_obj());
}

const std::string& as_string(const Value& value)
{
    return as_str(value)->str();
}

const char* as_cstr(const Value& value)
{
    return as_string(value).c_str();
}

bool is_func(const Value& value)
{
    return isType(value, ObjType::FUNCTION);
}

ObjFunction* as_func(const Value& value)
{
    return static_cast<ObjFunction*>(value.as_obj());
}

bool is_native(const Value& value)
{
    return isType(value, ObjType::NATIVE);
}

ObjNative* as_native(const Value& value)
{
    return static_cast<ObjNative*>(value.as_obj());
}

bool is_closure(const Value& value)
{
    return isType(value, ObjType::CLOSURE);
}

ObjClosure* as_closure(const Value& value)
{
    return static_cast<ObjClosure*>(value.as_obj());
}

bool is_class(const Value& value)
{
    return isType(value, ObjType::CLASS);
}

ObjClass* as_class(const Value& value)
{
    return static_cast<ObjClass*>(value.as_obj());
}

bool is_instance(const Value& value)
{
    return isType(value, ObjType::INSTANCE);
}

ObjInstance* as_instance(const Value& value)
{
    return static_cast<ObjInstance*>(value.as_obj());
}

bool is_bound_meth(const Value& value)
{
    return isType(value, ObjType::BOUND_METHOD);
}

ObjBoundMethod* as_bound_meth(const Value& value)
{
    return static_cast<ObjBoundMethod*>(value.as_obj());
}

template <Objective T>
bool is(const Value& value)
{
    if constexpr(std::is_same_v<T, Obj>)
        return value.is_obj();
    else if constexpr(std::is_same_v<T, ObjString>)
        return isType(value, ObjType::STRING);
    else if constexpr(std::is_same_v<T, ObjFunction>)
        return isType(value, ObjType::FUNCTION);
    else if constexpr(std::is_same_v<T, ObjNative>)
        return isType(value, ObjType::NATIVE);
    else if constexpr(std::is_same_v<T, ObjUpvalue>)
        return isType(value, ObjType::UPVALUE);
    else if constexpr(std::is_same_v<T, ObjClosure>)
        return isType(value, ObjType::CLOSURE);
    else if constexpr(std::is_same_v<T, ObjClass>)
        return isType(value, ObjType::CLASS);
    else if constexpr(std::is_same_v<T, ObjInstance>)
        return isType(value, ObjType::INSTANCE);
    else if constexpr(std::is_same_v<T, ObjBoundMethod>)
        return isType(value, ObjType::BOUND_METHOD);
    else
        return false;
}

template <Objective T>
T* as(const Value& value)
{
    return static_cast<T*>(value.as_obj());
}

template bool is<Obj>(const Value&);
template bool is<ObjString>(const Value&);
template bool is<ObjFunction>(const Value&);
template bool is<ObjNative>(const Value&);
template bool is<ObjUpvalue>(const Value&);
template bool is<ObjClosure>(const Value&);
template bool is<ObjClass>(const Value&);
template bool is<ObjInstance>(const Value&);
template bool is<ObjBoundMethod>(const Value&);

template Obj* as<Obj>(const Value&);
template ObjString* as<ObjString>(const Value&);
template ObjFunction* as<ObjFunction>(const Value&);
template ObjNative* as<ObjNative>(const Value&);
template ObjUpvalue* as<ObjUpvalue>(const Value&);
template ObjClosure* as<ObjClosure>(const Value&);
template ObjClass* as<ObjClass>(const Value&);
template ObjInstance* as<ObjInstance>(const Value&);
template ObjBoundMethod* as<ObjBoundMethod>(const Value&);

// ----

void printFunction(const ObjFunction* func)
{
    if(func->name.empty())
        std::printf("<script>");
    else
        std::printf("<fn %s>", func->name.c_str());
}

void printObject(const Value& value)
{
    if(!value.is_obj() || value.as_obj() == nullptr)
        return;

    switch (value.as_obj()->type)
    {
    case ObjType::FUNCTION:
        printFunction(as_func(value));
        break;
    case ObjType::STRING:
        std::printf("%s", as_cstr(value));
        break;
    case ObjType::NATIVE:
        std::printf("<native fn>");
        break;
    case ObjType::CLOSURE:
        printFunction(as_closure(value)->func);
        break;
    case ObjType::UPVALUE:
        printf("upvalue");
        break;
    case ObjType::CLASS:
        printf("%s", as_class(value)->name.c_str());
        break;
    case ObjType::INSTANCE:
        printf("%s instance", as_instance(value)->klass->name.c_str());
        break;
    case ObjType::BOUND_METHOD:
        printFunction(as_bound_meth(value)->method->func);
        break;
    default:
        std::printf("<object>");
        break;
    }
}

void printValue(const Value& value)
{
    if(value.is_nil())
        std::printf("nil");
    else if(value.is_number())
        std::printf("%g", value.as_number());
    else if(value.is_bool())
        std::printf(value.as_bool()? "true": "false");
    else if(value.is_obj())
        printObject(value);
    else
        std::printf("Unknown");
}

bool objectsEqual(Obj* left, Obj* right)
{
    if(left == right)
        return true;
    if(left == nullptr || right == nullptr || left->type != right->type)
        return false;
    if(left->type == ObjType::STRING)
        return static_cast<ObjString*>(left)->str() == static_cast<ObjString*>(right)->str();

    return false;
}
