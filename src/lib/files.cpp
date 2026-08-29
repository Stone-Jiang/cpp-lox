#include "files.h"

#include "../lang/object.h"
#include "../lang/vm.h"
#include "strings.h"

#include <array>
#include <cmath>
#include <filesystem>
#include <limits>
#include <new>
#include <stdexcept>

namespace
{
NativeResult badArity(std::string_view name, std::string_view expected,
    int actual, Value payload = Value())
{
    return NativeResult::failure(
        ErrorKind::CALL_ERROR,
        std::format("{}() expects {} arguments but got {}.", name, expected, actual),
        payload);
}

template<typename Action>
NativeResult fileAction(Value receiver, Action action)
{
    try
    {
        return action(as<ObjFile>(receiver)->file);
    }
    catch(const std::invalid_argument& error)
    {
        return NativeResult::failure(ErrorKind::VALUE_ERROR, error.what(), receiver);
    }
    catch(const std::filesystem::filesystem_error& error)
    {
        return NativeResult::failure(ErrorKind::IO_ERROR, error.what(), receiver);
    }
    catch(const std::ios_base::failure& error)
    {
        return NativeResult::failure(ErrorKind::IO_ERROR, error.what(), receiver);
    }
    catch(const std::bad_alloc&)
    {
        return NativeResult::failure(ErrorKind::CRITICAL_ERROR,
            "Not enough memory for file operation.", receiver);
    }
    catch(const std::length_error& error)
    {
        return NativeResult::failure(ErrorKind::RANGE_ERROR, error.what(), receiver);
    }
}

template<typename Integer>
NativeResult integerArgument(Value value, std::string_view description,
    Integer& result)
{
    if(!value.is_number())
        return NativeResult::failure(ErrorKind::TYPE_ERROR,
            std::format("{} must be an integral number.", description), value);

    const double number = value.as_number();
    if(!std::isfinite(number) || std::trunc(number) != number)
        return NativeResult::failure(ErrorKind::VALUE_ERROR,
            std::format("{} must be a finite integral number.", description), value);
    if(number < static_cast<double>(std::numeric_limits<Integer>::lowest()) ||
        number > static_cast<double>(std::numeric_limits<Integer>::max()))
        return NativeResult::failure(ErrorKind::RANGE_ERROR,
            std::format("{} is outside the supported range.", description), value);

    result = static_cast<Integer>(number);
    return NativeResult::success(Value());
}

NativeResult openFile(VM& vm, int argCount, Value* args)
{
    if(argCount < 1 || argCount > 2)
        return badArity("open", "1 or 2", argCount);
    if(!is<ObjString>(args[0]))
        return NativeResult::failure(ErrorKind::TYPE_ERROR,
            "open() path must be a string.", args[0]);
    if(argCount == 2 && !is<ObjString>(args[1]))
        return NativeResult::failure(ErrorKind::TYPE_ERROR,
            "open() mode must be a string.", args[1]);

    const std::string& path = as<ObjString>(args[0])->str();
    const std::string mode = argCount == 2
        ? as<ObjString>(args[1])->str()
        : "r";
    try
    {
        return NativeResult::success(Value(makeObj<ObjFile>(vm, path, mode)));
    }
    catch(const std::invalid_argument& error)
    {
        return NativeResult::failure(ErrorKind::VALUE_ERROR, error.what(), args[0]);
    }
    catch(const std::filesystem::filesystem_error& error)
    {
        return NativeResult::failure(ErrorKind::IO_ERROR, error.what(), args[0]);
    }
    catch(const std::ios_base::failure& error)
    {
        return NativeResult::failure(ErrorKind::IO_ERROR, error.what(), args[0]);
    }
    catch(const std::bad_alloc&)
    {
        return NativeResult::failure(ErrorKind::CRITICAL_ERROR,
            "Not enough memory to open file.", args[0]);
    }
}

NativeResult fileName(VM& vm, Value receiver)
{
    return makeStringResult(vm, as<ObjFile>(receiver)->file.name());
}

NativeResult fileMode(VM& vm, Value receiver)
{
    return makeStringResult(vm, as<ObjFile>(receiver)->file.mode());
}

NativeResult fileClosed(VM&, Value receiver)
{
    return NativeResult::success(Value(as<ObjFile>(receiver)->file.closed()));
}

NativeResult fileReadable(VM&, Value receiver)
{
    return NativeResult::success(Value(as<ObjFile>(receiver)->file.readable()));
}

NativeResult fileWritable(VM&, Value receiver)
{
    return NativeResult::success(Value(as<ObjFile>(receiver)->file.writable()));
}

NativeResult fileSeekable(VM&, Value receiver)
{
    return NativeResult::success(Value(as<ObjFile>(receiver)->file.seekable()));
}

NativeResult fileEof(VM&, Value receiver)
{
    return NativeResult::success(Value(as<ObjFile>(receiver)->file.eof()));
}

NativeResult fileSize(VM&, Value receiver)
{
    return fileAction(receiver, [](utils::File& file) {
        const auto size = file.size();
        if(size > static_cast<std::uintmax_t>(MAX_INDEX))
            return NativeResult::failure(ErrorKind::RANGE_ERROR,
                "File size is outside Lox's safe integer range.");
        return NativeResult::success(Value(static_cast<double>(size)));
    });
}

NativeResult fileRead(VM& vm, Value receiver, int argCount, Value* args)
{
    if(argCount > 1)
        return badArity("read", "0 or 1", argCount, receiver);

    std::streamsize count = -1;
    if(argCount == 1)
    {
        NativeResult converted = integerArgument(args[0], "read() size", count);
        if(!converted.ok)
            return converted;
    }
    return fileAction(receiver, [&](utils::File& file) {
        return makeStringResult(vm, file.read(count));
    });
}

NativeResult fileReadline(VM& vm, Value receiver, int, Value*)
{
    return fileAction(receiver, [&](utils::File& file) {
        auto line = file.readline();
        return line ? makeStringResult(vm, *line)
                    : NativeResult::success(Value());
    });
}

NativeResult fileReadlines(VM& vm, Value receiver, int, Value*)
{
    return fileAction(receiver, [&](utils::File& file) {
        auto lines = file.readlines();
        auto* result = makeObj<ObjArray>(vm);
        vm.push(Value(result));
        try
        {
            result->elements.reserve(lines.size());
            for(const auto& line: lines)
            {
                NativeResult string = makeStringResult(vm, line);
                if(!string.ok)
                {
                    vm.pop();
                    return string;
                }
                result->elements.push_back(string.value);
            }
        }
        catch(...)
        {
            vm.pop();
            throw;
        }
        vm.pop();
        return NativeResult::success(Value(result));
    });
}

NativeResult fileWrite(VM&, Value receiver, int, Value* args)
{
    if(!is<ObjString>(args[0]))
        return NativeResult::failure(ErrorKind::TYPE_ERROR,
            "write() expects a string.", args[0]);
    return fileAction(receiver, [&](utils::File& file) {
        return NativeResult::success(Value(static_cast<double>(
            file.write(as<ObjString>(args[0])->str()))));
    });
}

NativeResult fileWritelines(VM&, Value receiver, int, Value* args)
{
    if(!is<ObjArray>(args[0]))
        return NativeResult::failure(ErrorKind::TYPE_ERROR,
            "writelines() expects an array of strings.", args[0]);

    std::vector<std::string> lines;
    try
    {
        const auto* array = as<ObjArray>(args[0]);
        lines.reserve(array->len());
        for(const Value& value: array->elements)
        {
            if(!is<ObjString>(value))
                return NativeResult::failure(ErrorKind::TYPE_ERROR,
                    "writelines() expects every array element to be a string.", value);
            lines.push_back(as<ObjString>(value)->str());
        }
    }
    catch(const std::bad_alloc&)
    {
        return NativeResult::failure(ErrorKind::CRITICAL_ERROR,
            "Not enough memory for writelines().", args[0]);
    }
    catch(const std::length_error& error)
    {
        return NativeResult::failure(ErrorKind::RANGE_ERROR, error.what(), args[0]);
    }

    return fileAction(receiver, [&](utils::File& file) {
        file.writelines(lines);
        return NativeResult::success(Value());
    });
}

NativeResult fileSeek(VM&, Value receiver, int argCount, Value* args)
{
    if(argCount < 1 || argCount > 2)
        return badArity("seek", "1 or 2", argCount, receiver);

    std::streamoff offset;
    NativeResult converted = integerArgument(args[0], "seek() offset", offset);
    if(!converted.ok)
        return converted;
    int whence = 0;
    if(argCount == 2)
    {
        converted = integerArgument(args[1], "seek() whence", whence);
        if(!converted.ok)
            return converted;
    }
    return fileAction(receiver, [&](utils::File& file) {
        file.seek(offset, whence);
        return NativeResult::success(Value());
    });
}

NativeResult fileTell(VM&, Value receiver, int, Value*)
{
    return fileAction(receiver, [](utils::File& file) {
        const auto position = static_cast<std::streamoff>(file.tell());
        if(position < 0)
            return NativeResult::failure(ErrorKind::IO_ERROR,
                "tell() could not determine the current file position.");
        if(position > static_cast<std::streamoff>(MAX_INDEX))
            return NativeResult::failure(ErrorKind::RANGE_ERROR,
                "File position is outside Lox's safe integer range.");
        return NativeResult::success(Value(static_cast<double>(position)));
    });
}

NativeResult fileClose(VM&, Value receiver, int, Value*)
{
    return fileAction(receiver, [](utils::File& file) {
        file.close();
        return NativeResult::success(Value());
    });
}

NativeResult fileFlush(VM&, Value receiver, int, Value*)
{
    return fileAction(receiver, [](utils::File& file) {
        file.flush();
        return NativeResult::success(Value());
    });
}

constexpr std::array fileDefinitions {
    NativeDef{"open", -1, openFile},
};

constexpr std::array properties {
    NativePropertyDef{"name", fileName},
    NativePropertyDef{"mode", fileMode},
    NativePropertyDef{"closed", fileClosed},
    NativePropertyDef{"readable", fileReadable},
    NativePropertyDef{"writable", fileWritable},
    NativePropertyDef{"seekable", fileSeekable},
    NativePropertyDef{"eof", fileEof},
    NativePropertyDef{"size", fileSize},
};

constexpr std::array methods {
    NativeMethodDef{"read", -1, fileRead},
    NativeMethodDef{"readline", 0, fileReadline},
    NativeMethodDef{"readlines", 0, fileReadlines},
    NativeMethodDef{"write", 1, fileWrite},
    NativeMethodDef{"writelines", 1, fileWritelines},
    NativeMethodDef{"seek", -1, fileSeek},
    NativeMethodDef{"tell", 0, fileTell},
    NativeMethodDef{"close", 0, fileClose},
    NativeMethodDef{"flush", 0, fileFlush},
};
}

std::span<const NativeDef> fileNativeDefinitions() noexcept
{
    return fileDefinitions;
}

std::span<const NativePropertyDef> fileNativeProperties() noexcept
{
    return properties;
}

std::span<const NativeMethodDef> fileNativeMethods() noexcept
{
    return methods;
}

