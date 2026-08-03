#include "value.h"
#include "commons.h"

#ifdef NAN_BOXING



#else

Value& Value::operator=(std::monostate)
{
    data = std::monostate{};
    return *this;
}

Value& Value::operator=(double val)
{
    data = val;
    return *this;
}

Value& Value::operator=(bool val)
{
    data = val;
    return *this;
}

template <typename T>
T& Value::as() 
{
    try {
        return std::get<T>(data);
    } catch(const std::bad_variant_access& e) {
        throw std::runtime_error("Variant::as: type mismatch for" + std::string(typeid(T).name()));
    }
}

template <typename T>
const T& Value::as() const
{
    try {
        return std::get<T>(data);
    } catch(const std::bad_variant_access& e) {
        throw std::runtime_error("Variant::as: type mismatch for" + std::string(typeid(T).name()));
    }
}

bool Value::as_bool() const
{
    return as<bool>();
}

double Value::as_number() const
{
    return as<double>();
}

std::monostate Value::as_nil() const
{
    return as<std::monostate>();
}

Obj* Value::as_obj() const
{
    return as<Obj*>();
}

template <typename T>
bool Value::holds() const
{
    return std::holds_alternative<T>(data);
}

bool Value::is_bool() const
{
    return std::holds_alternative<bool>(data);
}
bool Value::is_number() const
{
    return std::holds_alternative<double>(data);
}
bool Value::is_nil() const
{
    return std::holds_alternative<std::monostate>(data);
}
bool Value::is_obj() const
{
    return std::holds_alternative<Obj*>(data);
}

size_t Value::index() const
{
    return data.index();
}

void Value::clear()
{
    data = std::monostate{};
}

void Value::swap(Value& other)
{
    data.swap(other.data);
}

template <typename Visitor>
auto Value::visit(Visitor&& visitor)
{
    return std::visit(std::forward<Visitor>(visitor), data);
}

template <typename Visitor>
auto Value::visit(Visitor&& visitor) const
{
    return std::visit(std::forward<Visitor>(visitor), data);
}

#ifdef DEBUG_VALUE_TYPENAME
std::string Value::type_name() const
{
    if(is_bool())
        return "bool";
    if(is_number())
        return "number";
    if(is_nil())
        return "nil";
    return "unknown";
}
#endif

std::string Value::to_string() const
{
    return std::visit([](auto&& value) -> std::string {
        using T = std::decay_t<decltype(value)>;
        if constexpr (std::is_same_v<T, std::monostate>)
            return "nil";
        else if constexpr (std::is_same_v<T, double>)
            return std::to_string(value);
        else if constexpr (std::is_same_v<T, bool>)
            return value? "true": "false";
        return "????";
    }, data);
}

bool Value::operator==(const Value& other) const
{
    if(is_obj() && other.is_obj())
        return objectsEqual(as_obj(), other.as_obj());
    return data==other.data;
}

bool Value::operator==(double val) const
{
    return is_number() && std::get<double>(data)==val;
}

bool Value::operator==(bool val) const
{
    return is_bool() && std::get<bool>(data)==val;
}

bool Value::operator==(std::monostate) const
{
    return is_nil();
}

#endif