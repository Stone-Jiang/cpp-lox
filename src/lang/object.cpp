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
    return value.is_obj() && value.is_obj()!=nullptr && value.as_obj()->type == type;
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
std::string formatNumber(double value)
{
    if(value == 0.0)
        value = 0.0;

    std::ostringstream output;
    output << std::setprecision(15) << value;
    return output.str();
}

std::string formatComplex(const std::complex<double>& value)
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

std::string objectToString(const Value& value)
{
    if(!value.is_obj() || value.as_obj() == nullptr)
        return "?";

    switch(value.as_obj()->type)
    {
    case ObjType::STRING:
        return as<ObjString>(value)->str();
    case ObjType::COMPLEX:
        return formatComplex(as<ObjComplex>(value)->c);
    case ObjType::ERROR:
    {
        const auto* error = as<ObjError>(value);
        return std::format("<err {}: {}>",
            errorKindName(error->kind), error->message->str());
    }
    default:
        return "????";
    }
}

void printFunction(const ObjFunction* func)
{
    if(func->name == nullptr)
        std::printf("<script>");
    else
        std::printf("<fn %s>", func->name->str().c_str());
}

void printObject(const Value& value)
{
    if(!value.is_obj() || value.as_obj() == nullptr)
        return;

    switch (value.as_obj()->type)
    {
    case ObjType::FUNCTION:
        printFunction(as<ObjFunction>(value));
        break;
    case ObjType::STRING:
        std::printf("%s", objectToString(value).c_str());
        break;
    case ObjType::NATIVE:
        std::printf("<native fn>");
        break;
    case ObjType::CLOSURE:
        printFunction(as<ObjClosure>(value)->func);
        break;
    case ObjType::UPVALUE:
        printf("upvalue");
        break;
    case ObjType::CLASS:
        printf("<cls %s>", as<ObjClass>(value)->name->str().c_str());
        break;
    case ObjType::ERROR:
        std::printf("%s", objectToString(value).c_str());
        break;
    case ObjType::INSTANCE:
        printf("<ins of cls %s>", as<ObjInstance>(value)->klass->name->str().c_str());
        break;
    case ObjType::BOUND_METHOD:
        printFunction(as<ObjBoundMethod>(value)->method->func);
        break;
    case ObjType::COMPLEX:
        std::printf("%s", objectToString(value).c_str());
        break;
    default:
        std::printf("<object>");
        break;
    }
}

void printValue(const Value& value)
{
    if(value.is_nil())
        std::printf("nil");
    else if(value.is_number())
        std::printf("%g", value.as_number());
    else if(value.is_bool())
        std::printf(value.as_bool()? "true": "false");
    else if(value.is_obj())
        printObject(value);
    else
        std::printf("Unknown");
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
    default:
        return false;
    }
}
