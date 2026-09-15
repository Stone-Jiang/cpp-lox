#include "value.h"
#include "commons.h"
#include "object.h"

#ifdef NAN_BOXING

Value::Value(double num)
{
    if(std::isnan(num))
        num = std::numeric_limits<double>::quiet_NaN();
    memcpy(&data, &num, sizeof(double));
}

Value::Value()
{
    data = NIL_VAL;
}

Value::Value(bool b)
{
    data = b? TRUE_VAL: FALSE_VAL;
}

Value::Value(Obj* obj)
{
    static_assert(sizeof(uintptr_t) <= sizeof(u64));
    const u64 pointer = (u64)(uintptr_t)(obj);
    if((pointer & ~POINTER_MASK) != 0)
        throw std::overflow_error("Object pointer does not fit in a NaN-boxed Value");
    data = SIGN_BIT | QNAN | pointer;
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

bool Value::operator<(const Value& other) const
{
    const auto rank = [](const Value& value) {
        if(value.is_nil()) return 0;
        if(value.is_bool()) return 1;
        if(value.is_number()) return 2;
        if(is<ObjString>(value)) return 3;
        return 4;
    };

    const int leftRank = rank(*this);
    const int rightRank = rank(other);
    if(leftRank != rightRank)
        return leftRank < rightRank;

    if(is_bool())
        return !as_bool() && other.as_bool();
    if(is_number())
    {
        const double left = as_number();
        const double right = other.as_number();
        if(std::isnan(left))
            return false;
        if(std::isnan(right))
            return true;
        return left < right;
    }
    if(is<ObjString>(*this))
        return as<ObjString>(*this)->str() < as<ObjString>(other)->str();

    // Nil and non-string objects are equivalent for ordering purposes.
    return false;
}
