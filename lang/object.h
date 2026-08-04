#pragma once
#include "value.h"
#include "chunk.h"
#include "commons.h"
#include "table.h"
#include <complex>

void allocObj(Obj* p);

enum class ObjType
{
    BOUND_METHOD, CLASS, CLOSURE, FUNCTION, INSTANCE, NATIVE, STRING, COMPLEX, UPVALUE, NOT
};

struct Obj
{
    ObjType type;
    Obj* next = nullptr;
    bool marked = false;

    Obj(ObjType type): type(type) 
    {
        allocObj(this);
    }

    Obj(): type(ObjType::NOT) 
    {
        allocObj(this);
    }
};

struct ObjString: Obj
{
    std::string chars;

    ObjString(std::string chars): Obj(ObjType::STRING), chars(std::move(chars)) {}

    const std::string& str() const
    { 
        return chars; 
    }
};

struct ObjComplex: Obj
{
    std::complex<double> c;

    ObjComplex(): Obj(ObjType::COMPLEX), c(0) {}
};

struct ObjFunction: Obj
{
    int arity = 0;
    int upvalCount = 0;
    Chunk chunk;
    std::string name = "";

    ObjFunction(): Obj(ObjType::FUNCTION) {}
};

typedef Value (*NativeFn)(int argCount, Value* args);

struct ObjNative: Obj
{
    NativeFn func;
    ObjNative(NativeFn nf): Obj(ObjType::NATIVE), func(nf) {}
};

struct ObjUpvalue: Obj
{
    Value* location = nullptr;
    Value closed;
    ObjUpvalue* next = nullptr;

    ObjUpvalue(Value* slot): Obj(ObjType::UPVALUE), location(slot), closed(Value()) {}
};

struct ObjClosure: Obj
{
    ObjFunction* func = nullptr;
    std::vector<ObjUpvalue*> upvalues;
    int upvalueCount;

    ObjClosure(ObjFunction* f): Obj(ObjType::CLOSURE)
    {
        for(int i=0; i<f->upvalCount; i++)
            upvalues.push_back(nullptr);
        
        this->func = f;
        this->upvalueCount = f->upvalCount;
    }
};

struct ObjClass: Obj
{
    std::string name;
    Table methods;

    ObjClass(std::string n): Obj(ObjType::CLASS), name(n) {}
};

struct ObjInstance: Obj
{
    ObjClass* klass = nullptr;
    Table fields;

    ObjInstance(ObjClass* klass): Obj(ObjType::INSTANCE), klass(klass) {}
};

struct ObjBoundMethod: Obj
{
    Value receiver;
    ObjClosure* method = nullptr;

    ObjBoundMethod(Value val, ObjClosure* c): Obj(ObjType::BOUND_METHOD), receiver(val), method(c) {} 
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
NativeFn as_native(const Value& value);
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

void printFunction(const ObjFunction* func);
void printObject(const Value& value);
void printValue(const Value& value);
bool objectsEqual(Obj* left, Obj* right);

ObjString* copyString(std::string_view chars);