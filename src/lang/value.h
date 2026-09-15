#pragma once
#include "commons.h"
#include "memory.h"

struct Obj;
struct ObjString;

bool objectsEqual(Obj* left, Obj* right);

class Value
{
    #ifdef NAN_BOXING

    u64 data;

    static constexpr u64 QNAN = 0x7ffc000000000000ULL;
    static constexpr u64 SIGN_BIT = 0x8000000000000000ULL;
    static constexpr u64 TAG_NIL = 1;
    static constexpr u64 TAG_FALSE = 2;
    static constexpr u64 TAG_TRUE = 3;
    static constexpr u64 NIL_VAL = QNAN | TAG_NIL;
    static constexpr u64 FALSE_VAL = QNAN | TAG_FALSE;
    static constexpr u64 TRUE_VAL = QNAN | TAG_TRUE;
    static constexpr u64 POINTER_MASK = ~(SIGN_BIT | QNAN);

    #else

    std::variant<std::monostate, double, bool, Obj*> data;

    #endif

public:
    #ifdef NAN_BOXING

    Value(double val);
    Value();
    Value(std::monostate): Value() {}
    Value(bool b);
    Value(Obj* obj);

    #else

    Value(): data(std::monostate{}) {}
    Value(std::monostate): data(std::monostate{}) {}
    Value(double val): data(val) {}
    Value(bool val): data(val) {}
    Value(Obj* p): data(p) {}
    
    template <typename T>
    T& as();
    template <typename T>
    const T& as() const;
    template <typename T>
    bool holds() const;

    #endif

    Value(const Value&) = default;
    Value(Value&&) = default;
    Value& operator=(const Value&) = default;
    Value& operator=(Value&&) = default;

    Value& operator=(std::monostate);
    Value& operator=(double val);
    Value& operator=(bool val);

    inline bool as_bool() const;
    inline double as_number() const;
    inline std::monostate as_nil() const;
    inline Obj* as_obj() const;

    inline bool is_bool() const;
    inline bool is_number() const;
    inline bool is_nil() const;
    inline bool is_obj() const;

    size_t index() const;
    void clear();
    void swap(Value& other);
    
    bool operator==(const Value& other) const;
    bool operator==(double val) const;
    bool operator==(bool val) const;
    bool operator==(std::monostate) const;

    bool operator<(const Value& other) const;
};

#ifdef NAN_BOXING

inline double Value::as_number() const
{
    if(!is_number())
        throw std::runtime_error("Value::as_number: type mismatch");
    double number;
    memcpy(&number, &data, sizeof(double));
    return number;
}

inline bool Value::is_number() const 
{ 
    return (data & QNAN) != QNAN; 
}

inline bool Value::is_nil() const 
{ 
    return data == NIL_VAL; 
}

inline std::monostate Value::as_nil() const
{
    if(!is_nil())
        throw std::runtime_error("Value::as_nil: type mismatch");
    return {};
}

inline bool Value::as_bool() const
{
    if(!is_bool())
        throw std::runtime_error("Value::as_bool: type mismatch");
    return data == TRUE_VAL;
}

inline bool Value::is_bool() const 
{ 
    return data == TRUE_VAL || data == FALSE_VAL; 
}

inline bool Value::is_obj() const
{
    return (data & (QNAN | SIGN_BIT)) == (QNAN | SIGN_BIT);
}

inline Obj* Value::as_obj() const
{
    if(!is_obj())
        throw std::runtime_error("Value::as_obj: type mismatch");
    return reinterpret_cast<Obj*>(static_cast<uintptr_t>(data & POINTER_MASK));
}

#else

template <typename T>
inline T& Value::as()
{
    try 
    {
        return std::get<T>(data);
    } 
    catch(const std::bad_variant_access&) {
        throw std::runtime_error("Variant::as: type mismatch for" + std::string(typeid(T).name()));
    }
}

template <typename T>
inline const T& Value::as() const
{
    try 
    {
        return std::get<T>(data);
    } 
    catch(const std::bad_variant_access&) 
    {
        throw std::runtime_error("Variant::as: type mismatch for" + std::string(typeid(T).name()));
    }
}

template <typename T>
inline bool Value::holds() const 
{ 
    return std::holds_alternative<T>(data); 
}

inline bool Value::as_bool() const 
{ 
    return as<bool>(); 
}

inline double Value::as_number() const 
{ 
    return as<double>(); 
}

inline std::monostate Value::as_nil() const 
{ 
    return as<std::monostate>(); 
}

inline Obj* Value::as_obj() const 
{ 
    return as<Obj*>(); 
}

inline bool Value::is_bool() const 
{ 
    return holds<bool>(); 
}

inline bool Value::is_number() const 
{
    return holds<double>(); 
}

inline bool Value::is_nil() const 
{ 
    return holds<std::monostate>(); 
}

inline bool Value::is_obj() const 
{ 
    return holds<Obj*>(); 
}

#endif

std::string to_string(const Value& value);
std::string to_string(const Obj* obj);
void printValue(const Value& value);

class ValueArray
{
public:
    Vector<Value> values;

    explicit ValueArray(VM* owner = nullptr): values(owner) {}

    void write(Value val)
    {
        values.push_back(val);
    }

    size_t size() const
    {
        return values.size();
    }

    Value& operator[](size_t i)
    {
        return values[i];
    }

    const Value& operator[](size_t i) const
    {
        return values[i];
    }
};
