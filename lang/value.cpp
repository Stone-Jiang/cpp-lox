#include "value.h"
#include "commons.h"

#ifdef NAN_BOXING

Value::Value(double num)
{
    if(std::isnan(num))
        num = std::numeric_limits<double>::quiet_NaN();
    memcpy(&data, &num, sizeof(double));
}

double Value::as_number() const
{
    if(!is_number())
        throw std::runtime_error("Value::as_number: type mismatch");

    double num;
    memcpy(&num, &data, sizeof(double));
    return num;
}

bool Value::is_number() const
{
    return (data&QNAN) != QNAN;
}

Value::Value()
{
    data = NIL_VAL;
}

bool Value::is_nil() const
{
    return data==NIL_VAL;
}

std::monostate Value::as_nil() const
{
    if(!is_nil())
        throw std::runtime_error("Value::as_nil: type mismatch");
    return std::monostate{};
}

Value::Value(bool b)
{
    data = b? TRUE_VAL: FALSE_VAL;
}

bool Value::as_bool() const
{
    if(!is_bool())
        throw std::runtime_error("Value::as_bool: type mismatch");
    return data==TRUE_VAL;
}

bool Value::is_bool() const
{
    return data==TRUE_VAL || data==FALSE_VAL;
}

Value::Value(Obj* obj)
{
    static_assert(sizeof(uintptr_t) <= sizeof(u64));
    const u64 pointer = (u64)(uintptr_t)(obj);
    if((pointer & ~POINTER_MASK) != 0)
        throw std::overflow_error("Object pointer does not fit in a NaN-boxed Value");
    data = SIGN_BIT | QNAN | pointer;
}

bool Value::is_obj() const
{
    return  (data & (QNAN | SIGN_BIT)) == (QNAN | SIGN_BIT);
}

Obj* Value::as_obj() const
{
    if(!is_obj())
        throw std::runtime_error("Value::as_obj: type mismatch");
    return (Obj*)(uintptr_t)(data & POINTER_MASK);
}

Value& Value::operator=(std::monostate)
{
    *this = Value();
    return *this;
}

Value& Value::operator=(double val)
{
    *this = Value(val);
    return *this;
}

Value& Value::operator=(bool val)
{
    *this = Value(val);
    return *this;
}

size_t Value::index() const
{
    if(is_nil())
        return 0;
    if(is_number())
        return 1;
    if(is_bool())
        return 2;
    if(is_obj())
        return 3;
    throw std::runtime_error("Invalid NaN-boxed Value");
}

void Value::clear()
{
    *this = Value();
}

void Value::swap(Value& other)
{
    std::swap(data, other.data);
}

std::string Value::to_string() const
{
    if(is_nil())
        return "nil";
    if(is_number())
        return std::to_string(as_number());
    if(is_bool())
        return as_bool()? "true": "false";
    if(is_obj())
        return objectToString(*this);
    return "????";
}

bool Value::operator==(const Value& other) const
{
    if(is_obj() && other.is_obj())
        return objectsEqual(as_obj(), other.as_obj());
    if(is_number() && other.is_number())
        return as_number() == other.as_number();
    return data==other.data;
}

bool Value::operator==(double val) const
{
    return is_number() && as_number()==val;
}

bool Value::operator==(bool val) const
{
    return is_bool() && as_bool()==val;
}

bool Value::operator==(std::monostate) const
{
    return is_nil();
}

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
    return holds<bool>();
}
bool Value::is_number() const
{
    return holds<double>();
}
bool Value::is_nil() const
{
    return holds<std::monostate>();
}
bool Value::is_obj() const
{
    return holds<Obj*>();
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

std::string Value::to_string() const
{
    if(is_obj())
        return objectToString(*this);

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
