#pragma once
#include "commons.h"

struct Obj;
struct ObjString;

bool objectsEqual(Obj* left, Obj* right);

class Value
{
    #ifdef NAN_BOXING
    u64 data;
    #else
    std::variant<std::monostate, double, bool, Obj*> data;
    #endif

public:
    Value(): data(std::monostate{}) {}
    Value(std::monostate): data(std::monostate{}) {}
    Value(double val): data(val) {}
    Value(bool val): data(val) {}
    Value(Obj* p): data(p) {}
    
    Value(const Value&) = default;
    Value(Value&&) = default;
    Value& operator=(const Value&) = default;
    Value& operator=(Value&&) = default;

    Value& operator=(std::monostate);
    Value& operator=(double val);
    Value& operator=(bool val);


    template <typename T>
    T& as();
    template <typename T>
    const T& as() const;

    bool as_bool() const;
    double as_number() const;
    std::monostate as_nil() const;
    Obj* as_obj() const;

    template <typename T>
    bool holds() const;

    bool is_bool() const;
    bool is_number() const;
    bool is_nil() const;
    bool is_obj() const;
    size_t index() const;
    void clear();
    void swap(Value& other);

    template <typename Visitor>
    auto visit(Visitor&& visitor);

    template <typename Visitor>
    auto visit(Visitor&& visitor) const;
    
    #ifdef DEBUG_VALUE_TYPENAME
    std::string type_name() const;
    #endif

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