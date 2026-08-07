#pragma once
#include "commons.h"

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

    bool as_bool() const;
    double as_number() const;
    std::monostate as_nil() const;
    Obj* as_obj() const;

    bool is_bool() const;
    bool is_number() const;
    bool is_nil() const;
    bool is_obj() const;
    size_t index() const;
    void clear();
    void swap(Value& other);

    std::string to_string() const;
    
    bool operator==(const Value& other) const;
    bool operator==(double val) const;
    bool operator==(bool val) const;
    bool operator==(std::monostate) const;
};

void printValue(const Value& value);

class ValueArray
{
public:
    std::vector<Value> values;

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
