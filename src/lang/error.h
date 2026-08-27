#pragma once

#include "commons.h"
#include "value.h"

enum class ErrorKind : u8
{
    DOMAIN_ERROR = 0,
    RANGE_ERROR,
    TYPE_ERROR,
    INDEX_ERROR,
    IO_ERROR,
    VALUE_ERROR,
    NAME_ERROR,
    USER_ERROR,
    CALL_ERROR,
    CRITICAL_ERROR
};

inline std::string_view errorKindName(ErrorKind kind) noexcept
{
    switch(kind)
    {
    case ErrorKind::DOMAIN_ERROR:
        return "DOMAIN";
    case ErrorKind::RANGE_ERROR:
        return "RANGE";
    case ErrorKind::TYPE_ERROR:
        return "TYPE";
    case ErrorKind::INDEX_ERROR:
        return "INDEX";
    case ErrorKind::IO_ERROR:
        return "IO";
    case ErrorKind::VALUE_ERROR:
        return "VALUE";
    case ErrorKind::NAME_ERROR:
        return "NAME";
    case ErrorKind::CALL_ERROR:
        return "CALL";
    case ErrorKind::CRITICAL_ERROR:
        return "CRITICAL";
    case ErrorKind::USER_ERROR:
        return "USER";
    }

    return "UNKNOWN";
}

struct NativeError
{
    ErrorKind kind = ErrorKind::USER_ERROR;
    std::string message;
    Value payload;
};

