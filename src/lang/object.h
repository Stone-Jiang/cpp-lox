#pragma once
#include "value.h"
#include "chunk.h"
#include "commons.h"
#include "table.h"
#include "native.h"
#include "valuetable.h"
#include <complex>
#include <fstream>
#include <utility>
#include <concepts>

void prepareObjAlloc(VM* owner, size_t size);
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
    FILE,
    MAP,
    NONE // for debug only
};

struct Obj
{
    static constexpr ObjType basetype = ObjType::OBJ;

    VM* owner = nullptr;
    Obj* next = nullptr;
    bool marked = false;
    ObjType type;

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

    Chunk chunk;
    ObjString* name = nullptr;
    int arity = 0;
    int upvalCount = 0;
    bool variadic = false;
    
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

    Vector<ObjUpvalue*> upvalues;
    ObjFunction* func = nullptr;
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

    MemberTable members;
    ObjString* name = nullptr;
    ObjClass* superclass = nullptr;

    ObjClass(VM* owner, ObjString* name): Obj(owner, basetype), members(owner), name(name) {}
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

    Table fields;
    ObjClass* klass = nullptr;

    ObjInstance(VM* owner, ObjClass* klass): 
        Obj(owner, basetype), fields(owner), klass(klass) {}
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

struct ObjFile: Obj
{
    static constexpr ObjType basetype = ObjType::FILE;

    utils::File file;

    ObjFile(VM* owner, const std::string& path, const std::string& mode): Obj(owner, ObjType::FILE), file(path, mode) {}
};

struct ObjMap: Obj
{
    static constexpr ObjType basetype = ObjType::MAP;

    ValueTable vt;

    ObjMap(VM* owner): Obj(owner, basetype), vt(owner) {}

    inline size_t len() const
    {
        return vt.size();
    }
};

// -----

#ifndef RELEASE_UNCHECKED_CAST

inline ObjType objType(const Value& value)
{
    if(value.is_obj())
        return value.as_obj()->type;
    return ObjType::NONE;
}

#else

inline ObjType objType(const Value& value)
{
    return value.as_obj()->type;
}

#endif

inline bool isType(const Value& value, ObjType type)
{
    return value.is_obj() && value.as_obj()!=nullptr && value.as_obj()->type == type;
}

template <class T>
concept Objective = std::is_base_of_v<Obj, T>;

template <Objective T>
inline bool is(const Value& value)
{
    return isType(value, T::basetype);
}

#ifndef RELEASE_UNCHECKED_CAST

template <Objective T>
inline T* as(const Value& value)
{
    if(!value.is_obj())
        throw std::invalid_argument("value cannot be cast");
    return static_cast<T*>(value.as_obj());
}

#else

template <Objective T>
inline T* as(const Value& value)
{
    return static_cast<T*>(value.as_obj());
}

#endif

template <Objective T>
inline T* as(Obj* obj)
{
    return static_cast<T*>(obj);
}

template <Objective T>
inline const T* as(const Obj* obj)
{
    return static_cast<const T*>(obj);
}

inline const string& as_string(const Value& value)
{
    return as<ObjString>(value)->str();
}

inline const char* as_cstr(const Value& value)
{
    return as_string(value).c_str();
}

// -----

template <Objective T, typename... Args>
T* makeObj(VM& owner, Args&&... args)
{
    constexpr size_t size = sizeof(T);
    prepareObjAlloc(&owner, size);

    T* object = new T(&owner, std::forward<Args>(args)...);
    allocObj(&owner, object, size);
    return object;
}

std::string to_string(const Value& value);
std::string to_string(const Obj* obj);
std::string to_string(const ObjFunction* func);
std::string to_string(const ObjArray* array);
std::string to_string(const ObjMap* map);

void printValue(const Value& value);

std::string type_string(const Value& value);

bool objectsEqual(Obj* left, Obj* right);

ObjString* copyString(VM& owner, std::string_view chars);

bool is_integral(const Value& value);
