#include "object.h"

#include <iomanip>

ObjType objType(const Value& value)
{
    if(value.is_obj())
        return value.as_obj()->type;
    return ObjType::NONE;
}

bool isType(const Value& value, ObjType type)
{
    return value.is_obj() && value.as_obj()!=nullptr && value.as_obj()->type == type;
}

const string& as_string(const Value& value)
{
    return as<ObjString>(value)->str();
}

const char* as_cstr(const Value& value)
{
    return as_string(value).c_str();
}

// -----

namespace
{
string formatNumber(double value)
{
    if(value == 0.0)
        value = 0.0;

    std::ostringstream output;
    output << std::setprecision(15) << value;
    return output.str();
}

string formatComplex(const std::complex<double>& value)
{
    const double real = value.real();
    const double imag = value.imag();

    if(imag == 0.0)
        return formatNumber(real);
    if(real == 0.0)
        return formatNumber(imag) + "i";

    return formatNumber(real) +
        (imag >= 0.0 ? "+" : "") +
        formatNumber(imag) + "i";
}
}

bool objectsEqual(Obj* left, Obj* right)
{
    if(left == right)
        return true;
    if(left == nullptr || right == nullptr || left->type != right->type)
        return false;
    switch(left->type)
    {
    case ObjType::STRING:
        return static_cast<ObjString*>(left)->str() ==
            static_cast<ObjString*>(right)->str();
    case ObjType::COMPLEX:
        return static_cast<ObjComplex*>(left)->c ==
            static_cast<ObjComplex*>(right)->c;
    case ObjType::ARRAY:
        return false; //! Update
    default:
        return false;
    }
}

std::string to_string(const Value& value)
{
    if(value.is_nil())
        return ("nil");
    else if(value.is_number())
        return formatNumber(value.as_number());
    else if(value.is_bool())
        return value.as_bool()? "true": "false";
    else if(value.is_obj())
        return to_string(value.as_obj());
    else
        return "unknown value"; // unreachable
}

std::string to_string(const Obj* obj)
{
    if(obj==nullptr)
        return "?";
    switch (obj->type)
    {
    case ObjType::FUNCTION:
        return to_string(as<ObjFunction>(obj));
    case ObjType::STRING:
        return as<ObjString>(obj)->str();
    case ObjType::NATIVE:
        return "<native fn>";
    case ObjType::CLOSURE:
        return to_string(as<ObjClosure>(obj)->func);
    case ObjType::UPVALUE:
        return "upvalue";
        break;
    case ObjType::CLASS:
        return "<cls " + as<ObjClass>(obj)->name->str() + ">";
    case ObjType::ERROR:
    {
        const auto* error = as<ObjError>(obj);
        return std::format("<err {}: {}>", errorKindName(error->kind), error->message->str());
    }
    case ObjType::INSTANCE:
        return "<ins of cls " + as<ObjInstance>(obj)->klass->name->str() + ">";
    case ObjType::BOUND_METHOD:
        return to_string(as<ObjBoundMethod>(obj)->method->func);
    case ObjType::COMPLEX:
        return formatComplex(as<ObjComplex>(obj)->c);
    case ObjType::ARRAY:
    {
        const auto& vec = as<ObjArray>(obj)->elements;
        string s;
        s.reserve(vec.size()*3);
        s += "[";
        for (size_t i = 0; i < vec.size()-1; i++)
        {
            s += to_string(vec[i]);
            s += ", ";
        }
        s += to_string(vec[vec.size()-1]);
        s += "]";
        return s;
    }
    default:
        return "<obj ?>";
    }
}

std::string to_string(const ObjFunction* func)
{
    if(func->name == nullptr)
        return "<script>";
    else
        return "<fn %s>" + func->name->str();
}

void printValue(const Value& value)
{
    std::printf("%s", to_string(value).c_str());
}

