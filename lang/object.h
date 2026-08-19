#pragma once
#include "value.h"
#include "chunk.h"
#include "commons.h"
#include "table.h"
#include "native.h"
#include <complex>
#include <utility>

void prepareObjAllocation(VM* owner, size_t size);
void allocObj(VM* owner, Obj* object, size_t size);

enum class ObjType
{
    BOUND_METHOD, CLASS, CLOSURE, FUNCTION, INSTANCE, NATIVE, STRING, COMPLEX, UPVALUE, NOT
};

struct Obj
{
    VM* owner = nullptr;
    ObjType type;
    Obj* next = nullptr;
    bool marked = false;

    Obj(VM* owner, ObjType type): owner(owner), type(type) {}

    Obj(): type(ObjType::NOT) {}
};

struct ObjString: Obj
{
    const std::string chars;
    const size_t hash;

    ObjString(VM* owner, std::string chars):
        Obj(owner, ObjType::STRING), chars(std::move(chars)),
        hash(std::hash<std::string_view>{}(this->chars)) {}

    const std::string& str() const
    { 
        return chars; 
    }
};

struct ObjComplex: Obj
{
    std::complex<double> c;

    ObjComplex(VM* owner): Obj(owner, ObjType::COMPLEX), c(0) {}
    ObjComplex(VM* owner, double r, double i): Obj(owner, ObjType::COMPLEX), c(r, i) {}
};

struct ObjFunction: Obj
{
    int arity = 0;
    int upvalCount = 0;
    Chunk chunk;
    ObjString* name = nullptr;

    ObjFunction(VM* owner, ObjString* functionName = nullptr):
        Obj(owner, ObjType::FUNCTION), chunk(owner), name(functionName) {}
};

struct ObjNative: Obj
{
    NativeFn func;
    int arity;

    ObjNative(VM* owner, NativeFn nf, int a):
        Obj(owner, ObjType::NATIVE), func(nf), arity(a) {}
};

struct ObjUpvalue: Obj
{
    Value* location = nullptr;
    Value closed;
    ObjUpvalue* next = nullptr;

    ObjUpvalue(VM* owner, Value* slot):
        Obj(owner, ObjType::UPVALUE), location(slot), closed(Value()) {}
};

struct ObjClosure: Obj
{
    ObjFunction* func = nullptr;
    Vector<ObjUpvalue*> upvalues;
    int upvalueCount;

    ObjClosure(VM* owner, ObjFunction* f):
        Obj(owner, ObjType::CLOSURE), upvalues(owner)
    {
        for(int i=0; i<f->upvalCount; i++)
            upvalues.push_back(nullptr);
        
        this->func = f;
        this->upvalueCount = f->upvalCount;
    }
};

struct ObjClass: Obj
{
    ObjString* name = nullptr;
    Table methods;

    ObjClass(VM* owner, ObjString* name):
        Obj(owner, ObjType::CLASS), name(name), methods(owner) {}
};

struct ObjInstance: Obj
{
    ObjClass* klass = nullptr;
    Table fields;

    ObjInstance(VM* owner, ObjClass* klass):
        Obj(owner, ObjType::INSTANCE), klass(klass), fields(owner) {}
};

struct ObjBoundMethod: Obj
{
    Value receiver;
    ObjClosure* method = nullptr;

    ObjBoundMethod(VM* owner, Value val, ObjClosure* c):
        Obj(owner, ObjType::BOUND_METHOD), receiver(val), method(c) {}
};

std::optional<ObjType> objType(const Value& value);
bool isType(const Value& value, ObjType type);

bool is_str(const Value& value);
const ObjString* as_str(const Value& value);
const std::string& as_string(const Value& value);
const char* as_cstr(const Value& value);
bool is_func(const Value& value);
ObjFunction* as_func(const Value& value);
bool is_native(const Value& value);
ObjNative* as_native(const Value& value);
bool is_closure(const Value& value);
ObjClosure* as_closure(const Value& value);
bool is_class(const Value& value);
ObjClass* as_class(const Value& value);
bool is_instance(const Value& value);
ObjInstance* as_instance(const Value& value);
bool is_bound_meth(const Value& value);
ObjBoundMethod* as_bound_meth(const Value& value);

template <class T>
concept Objective = std::is_base_of_v<Obj, T>;

template <Objective T>
bool is(const Value& value);

template <Objective T>
T* as(const Value& value);

template <Objective T, typename... Args>
T* makeObj(VM& owner, Args&&... args)
{
    constexpr size_t size = sizeof(T);
    prepareObjAllocation(&owner, size);

    T* object = new T(&owner, std::forward<Args>(args)...);
    allocObj(&owner, object, size);
    return object;
}

void printFunction(const ObjFunction* func);
void printObject(const Value& value);
void printValue(const Value& value);
bool objectsEqual(Obj* left, Obj* right);

ObjString* copyString(VM& owner, std::string_view chars);
