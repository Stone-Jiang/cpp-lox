#pragma once
#include "value.h"
#include "chunk.h"
#include "commons.h"
#include "table.h"
#include "native.h"
#include <complex>
#include <utility>
#include <concepts>

void prepareObjAllocation(VM* owner, size_t size);
void allocObj(VM* owner, Obj* object, size_t size);

enum class ObjType
{
    OBJ, // for debug only
    BOUND_METHOD, 
    CLASS, 
    CLOSURE, 
    ERROR,
    FUNCTION, 
    INSTANCE, 
    NATIVE, 
    STRING, 
    COMPLEX, 
    UPVALUE,
    ARRAY,
    NONE // for debug only
};

struct Obj
{
    static constexpr ObjType basetype = ObjType::OBJ;

    VM* owner = nullptr;
    ObjType type;
    Obj* next = nullptr;
    bool marked = false;

    Obj(VM* owner, ObjType type): owner(owner), type(type) {}
};

struct ObjString: Obj
{
    static constexpr ObjType basetype = ObjType::STRING;
    const std::string chars;
    const size_t hash;

    ObjString(VM* owner, std::string chars): Obj(owner, basetype), chars(std::move(chars)),
        hash(std::hash<std::string_view>{}(this->chars)) {}

    const std::string& str() const
    { 
        return chars; 
    }
};

struct ObjComplex: Obj
{
    static constexpr ObjType basetype = ObjType::COMPLEX;
    std::complex<double> c;

    ObjComplex(VM* owner): Obj(owner, basetype), c(0, 0) {}
    ObjComplex(VM* owner, double r, double i): Obj(owner, basetype), c(r, i) {}
};

struct ObjFunction: Obj
{
    static constexpr ObjType basetype = ObjType::FUNCTION;
    int arity = 0;
    int upvalCount = 0;
    Chunk chunk;
    ObjString* name = nullptr;

    ObjFunction(VM* owner, ObjString* functionName = nullptr):
        Obj(owner, basetype), chunk(owner), name(functionName) {}
};

struct ObjNative: Obj
{
    static constexpr ObjType basetype = ObjType::NATIVE;
    NativeFn func;
    int arity;

    ObjNative(VM* owner, NativeFn nf, int a): Obj(owner, basetype), func(nf), arity(a) {}
};

struct ObjUpvalue: Obj
{
    static constexpr ObjType basetype = ObjType::UPVALUE;
    Value* location = nullptr;
    Value closed;
    ObjUpvalue* next = nullptr;

    ObjUpvalue(VM* owner, Value* slot): Obj(owner, basetype), location(slot), closed(Value()) {}
};

struct ObjClosure: Obj
{
    static constexpr ObjType basetype = ObjType::CLOSURE;
    ObjFunction* func = nullptr;
    Vector<ObjUpvalue*> upvalues;
    int upvalueCount;

    ObjClosure(VM* owner, ObjFunction* f): Obj(owner, basetype), upvalues(owner)
    {
        for(int i=0; i<f->upvalCount; i++)
            upvalues.push_back(nullptr);
        
        this->func = f;
        this->upvalueCount = f->upvalCount;
    }
};

struct ObjClass: Obj
{
    static constexpr ObjType basetype = ObjType::CLASS;
    ObjString* name = nullptr;
    ObjClass* superclass = nullptr;
    MemberTable members;

    ObjClass(VM* owner, ObjString* name): Obj(owner, basetype), name(name), members(owner) {}
};

struct ObjError: Obj
{
    static constexpr ObjType basetype = ObjType::ERROR;
    ErrorKind kind = ErrorKind::USER_ERROR;
    ObjString* message = nullptr;
    Value payload;

    ObjError(VM* owner, ErrorKind kind, ObjString* message, Value payload = Value()):
        Obj(owner, basetype), kind(kind), message(message), payload(payload) {}
};

struct ObjInstance: Obj
{
    static constexpr ObjType basetype = ObjType::INSTANCE;
    ObjClass* klass = nullptr;
    Table fields;

    ObjInstance(VM* owner, ObjClass* klass): 
        Obj(owner, basetype), klass(klass), fields(owner) {}
};

struct ObjBoundMethod: Obj
{
    static constexpr ObjType basetype = ObjType::BOUND_METHOD;
    Value receiver;
    ObjClosure* method = nullptr;

    ObjBoundMethod(VM* owner, Value val, ObjClosure* c): 
        Obj(owner, basetype), receiver(val), method(c) {}
};

struct ObjArray: Obj
{
    static constexpr ObjType basetype = ObjType::ARRAY;
    Vector<Value> elements;

    inline size_t len() const
    {
        return elements.size();
    }

    ObjArray(VM* owner): Obj(owner, basetype), elements(owner) {}
};


ObjType objType(const Value& value);
bool isType(const Value& value, ObjType type);

template <class T>
concept Objective = std::is_base_of_v<Obj, T>;

template <Objective T>
bool is(const Value& value)
{
    return isType(value, T::basetype);
}

template <Objective T>
T* as(const Value& value)
{
    if(!value.is_obj())
        throw std::invalid_argument("value cannot be cast");
    return static_cast<T*>(value.as_obj());
}

template <Objective T>
T* as(Obj* obj)
{
    return static_cast<T*>(obj);
}

template <Objective T>
const T* as(const Obj* obj)
{
    return static_cast<const T*>(obj);
}

const string& as_string(const Value& value);
const char* as_cstr(const Value& value);

// -----

template <Objective T, typename... Args>
T* makeObj(VM& owner, Args&&... args)
{
    constexpr size_t size = sizeof(T);
    prepareObjAllocation(&owner, size);

    T* object = new T(&owner, std::forward<Args>(args)...);
    allocObj(&owner, object, size);
    return object;
}

std::string to_string(const Value& value);
std::string to_string(const Obj* obj);

std::string to_string(const ObjFunction* func);
void printValue(const Value& value);

bool objectsEqual(Obj* left, Obj* right);

ObjString* copyString(VM& owner, std::string_view chars);

bool is_integral(const Value& value);