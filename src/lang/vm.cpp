#include "vm.h"
#include "../lib/files.h"
#include "../lib/maps.h"
#include "../lib/math.h"
#include "../lib/stats.h"
#include "../utils/color.h"
#include <algorithm>

Compiler Compiler::comp{};

using std::complex;

namespace
{
bool isNumeric(const Value& value)
{
    return value.is_number() || is<ObjComplex>(value);
}

bool isFalsy(Value value)
{
    return value.is_nil() || (value.is_bool() && !value.as_bool());
}

complex<double> asComplexNumber(const Value& value)
{
    return value.is_number()? 
        complex<double>(value.as_number(), 0.0): 
        as<ObjComplex>(value)->c;
}

bool approxEqual(complex<double> left, complex<double> right)
{
    if(left == right)
        return true;

    if(!std::isfinite(left.real()) || !std::isfinite(left.imag()) ||
        !std::isfinite(right.real()) || !std::isfinite(right.imag()))
        return false;

    const double scale = std::max({1.0, std::abs(left), std::abs(right)});
    return std::abs(left - right) <= APPROX_EPSILON * scale;
}

struct ArrayComparison
{
    const ObjArray* left;
    const ObjArray* right;
    const ArrayComparison* parent;
};

bool approxEqual(const Value& left, const Value& right, const ArrayComparison* comparisonPath)
{
    if(isNumeric(left) && isNumeric(right))
        return approxEqual(asComplexNumber(left), asComplexNumber(right));

    const bool leftIsArray = is<ObjArray>(left);
    const bool rightIsArray = is<ObjArray>(right);
    if(leftIsArray != rightIsArray)
        return false;

    if(!leftIsArray)
        return left == right;

    const auto* leftArray = as<ObjArray>(left);
    const auto* rightArray = as<ObjArray>(right);
    if(leftArray == rightArray)
        return true;
    if(leftArray->elements.size() != rightArray->elements.size())
        return false;

    for(const ArrayComparison* comparison = comparisonPath;
        comparison != nullptr;
        comparison = comparison->parent)
    {
        if(comparison->left == leftArray && comparison->right == rightArray)
            return true;
    }

    // This node lives in the current recursive stack frame. It prevents cycles
    // without allocating a separate container or changing either array.
    const ArrayComparison current{leftArray, rightArray, comparisonPath};
    for(size_t i = 0; i < leftArray->elements.size(); ++i)
    {
        if(!approxEqual(
            leftArray->elements[i], rightArray->elements[i], &current))
            return false;
    }
    return true;
}

size_t saturatingAdd(size_t left, size_t right)
{
    const size_t max = std::numeric_limits<size_t>::max();
    return right > max - left ? max : left + right;
}

size_t objectExtraBytes(const Obj* object)
{
    switch(object->type)
    {
    case ObjType::STRING:
        return saturatingAdd(as<ObjString>(object)->chars.capacity(), 1);
    case ObjType::ARRAY:
        return 0;
    default:
        return 0;
    }
}

size_t nextCollectionThresh(size_t liveBytes)
{
    const size_t max = std::numeric_limits<size_t>::max();
    const size_t grown = liveBytes > max / HEAP_GROW_FACTOR? 
        max: liveBytes * HEAP_GROW_FACTOR;
    return std::max(GC_MIN_THRESH, grown);
}

bool decodeErrorKind(Value value, ErrorKind& kind)
{
    if(!value.is_number())
        return false;

    const double raw = value.as_number();
    const double integral = std::floor(raw);
    if(!std::isfinite(raw) || raw != integral)
        return false;

    switch(static_cast<int>(integral))
    {
    case static_cast<int>(ErrorKind::DOMAIN_ERROR):
        kind = ErrorKind::DOMAIN_ERROR;
        return true;
    case static_cast<int>(ErrorKind::RANGE_ERROR):
        kind = ErrorKind::RANGE_ERROR;
        return true;
    case static_cast<int>(ErrorKind::TYPE_ERROR):
        kind = ErrorKind::TYPE_ERROR;
        return true;
    case static_cast<int>(ErrorKind::INDEX_ERROR):
        kind = ErrorKind::INDEX_ERROR;
        return true;
    case static_cast<int>(ErrorKind::IO_ERROR):
        kind = ErrorKind::IO_ERROR;
        return true;
    case static_cast<int>(ErrorKind::VALUE_ERROR):
        kind = ErrorKind::VALUE_ERROR;
        return true;
    case static_cast<int>(ErrorKind::NAME_ERROR):
        kind = ErrorKind::NAME_ERROR;
        return true;
    case static_cast<int>(ErrorKind::USER_ERROR):
        kind = ErrorKind::USER_ERROR;
        return true;
    default:
        return false;
    }
}
}

NormalIndexInfo normalIndex(const Value& value, size_t length)
{
    if(!value.is_number())
        return {0, IndexResult::NOT_NUMBER};

    const double raw = value.as_number();
    if(!std::isfinite(raw))
        return {0, IndexResult::NOT_FINITE};
    if(std::trunc(raw)!=raw)
        return {0, IndexResult::NOT_INTEGRAL};
    if(std::abs(raw)>MAX_INDEX)
        return {0, IndexResult::NOT_SAFE};
    
    const i64 index = static_cast<i64>(raw);

    if(index>=0)
    {
        const size_t pos = static_cast<size_t>(index);
        if(pos>length)
            return {0, IndexResult::OUT_OF_RANGE}; 
        if(pos==length)
            return {pos, IndexResult::END};
        return {pos, IndexResult::OK};
    }
    else
    {
        const size_t dist = static_cast<size_t>(-index);
        if(dist>length)
            return {0, IndexResult::OUT_OF_RANGE};
        return {length-dist, IndexResult::OK};
    }
}

class TempRootGuard
{
    VM& vm;
    Obj* prev;
public:
    TempRootGuard(VM& vm, Obj* obj): vm(vm), prev(vm.temporaryRoot)
    {
        vm.temporaryRoot = obj;
    }
    ~TempRootGuard()
    {
        vm.temporaryRoot = prev;
    }

    TempRootGuard(const TempRootGuard& other) = delete;
    TempRootGuard& operator=(const TempRootGuard& other) = delete;
};

VM::VM():globals(this), strings(this), grayStack(this),
    arrayExt(this), stringExt(this), mapExt(this), functionExt(this)
{
    const auto natives = nativeDefinitions();
    globals.reserve(natives.size());

    initStr = copyString(*this, "init");

    for(const auto& nat: natives)
        defineNative(nat);
    for(const auto& nat: mathNativeDefinitions())
        defineNative(nat);
    for(const auto& nat: statsNativeDefinitions())
        defineNative(nat);
    for(const auto& nat: fileNativeDefinitions())
        defineNative(nat);
    for(const auto& nat: mapNativeDefinitions())
        defineNative(nat);
}

VM::~VM()
{
    gcEnabled = false;
    resetStack();
    freeObjs();
}

Result VM::interpret(const string& src)
{
    gcEnabled = true;

    resetStack();

    auto* func = Compiler::comp.compile(*this, src);
    if(func == nullptr)
        return Result::COMPILE_ERROR;

    push(Value(func));
    auto clos = makeObj<ObjClosure>(*this, func);
    pop();
    push(Value(clos));
    call(clos, 0);

    return run();
}

Result VM::run()
{
    auto frame = &frames[frameCount-1];

    while(true) 
    {
        #ifdef DEBUG_TRACE_EXECUTION
        printf("          ");
        for(auto it=stack.data(); it!=stackTop; ++it)
        {
            utils::TerminalColor<utils::Color::Green> color;
            utils::TerminalColor<utils::Color::Dim> style;
            printf("[ ");
            printValue(*it);
            printf(" ]");
        }
        printf("\n");
        Debug::disassembleInstruction(frame->clos->func->chunk, 
            static_cast<int>(frame->ip - frame->clos->func->chunk.code.data()));
        #endif

        const auto instruction = read_byte(frame);
        switch(instruction) 
        {
        case OpCode::CONSTANT:
        {
            Value constant = read_constant(frame);
            push(constant);
            break;
        }

        case OpCode::ADD:
        {
            if(rejectError(peek(0), "Can't use an error value in addition.") || rejectError(peek(1), "Can't use an error value in addition."))
                return Result::RUNTIME_ERROR;

            if(is<ObjString>(peek(0)) && is<ObjString>(peek(1)))
                concat();
            else if(peek(0).is_number() && peek(1).is_number())
            {
                if(!binaryOp([](double a, double b) {return a+b;}))
                    return Result::RUNTIME_ERROR;
            }
            else if(isNumeric(peek(0)) && isNumeric(peek(1)))
            {
                if(!complexBinaryOp([](complex<double> a, complex<double> b) { return a+b; }))
                    return Result::RUNTIME_ERROR;
            }
            else
            {
                runtimeError("Operands must be numbers, complex numbers, or strings.");
                return Result::RUNTIME_ERROR;
            }
            break;
        }
        
        case OpCode::SUBTRACT:
            if(peek(0).is_number() && peek(1).is_number())
            {
                if(!binaryOp([](double a, double b) {return a-b;}))
                    return Result::RUNTIME_ERROR;
            }
            else if(!complexBinaryOp([](complex<double> a, complex<double> b) {return a-b;}))
                return Result::RUNTIME_ERROR;
            break;
        case OpCode::MULTIPLY:
            if(peek(0).is_number() && peek(1).is_number())
            {
                if(!binaryOp([](double a, double b) {return a*b;}))
                    return Result::RUNTIME_ERROR;
            }
            else if(!complexBinaryOp([](complex<double> a, complex<double> b) {return a*b;}))
                return Result::RUNTIME_ERROR;
            break;
        case OpCode::DIVIDE:
            if(peek(0).is_number() && peek(1).is_number())
            {
                if(!binaryOp([](double a, double b) {return a/b;}))
                    return Result::RUNTIME_ERROR;
            }
            else if(!complexBinaryOp([](complex<double> a, complex<double> b) {return a/b;}))
                return Result::RUNTIME_ERROR;
            break;
        case OpCode::GREATER:
            if(!binaryOp([](double a, double b) {return a>b;}))
                return Result::RUNTIME_ERROR;
            break;
        case OpCode::LESS:
            if(!binaryOp([](double a, double b) {return a<b;}))
                return Result::RUNTIME_ERROR;
            break;
        case OpCode::APPROX:
        {
            std::string_view msg = "Can't compare an error value directly.";
            if(rejectError(peek(0), msg) || rejectError(peek(1), msg))
                return Result::RUNTIME_ERROR;

            const bool equal = approxEqual(peek(1), peek(0), nullptr);
            pop();
            pop();
            push(Value(equal));

            break;
        }

        case OpCode::NOT:
            push(Value(isFalsy(pop()))); break;
        case OpCode::NIL:
            push(Value()); break;
        case OpCode::TRUE:
            push(Value(true)); break;
        case OpCode::FALSE:
            push(Value(false)); break;
        
        case OpCode::POP:
            pop(); break;
        case OpCode::POP_UNHANDLED:
            if(rejectError(peek(0), "Unhandled error result."))
                return Result::RUNTIME_ERROR;
            pop();
            break;
        case OpCode::EQUAL:
        {
            std::string_view msg = "Can't compare an error value directly.";
            if(rejectError(peek(0), msg) || rejectError(peek(1), msg))
            {
                Value err = makeErrorResult(ErrorKind::TYPE_ERROR, msg, Value(false));
                pop();
                pop();
                push(err);
            }

            Value b = pop();
            Value a = pop();
            push(Value(a==b));
            break;
        }

        case OpCode::NEGATE:
        {
            if(rejectError(peek(0), "Can't negate an error value."))
                return Result::RUNTIME_ERROR;
            if(!isNumeric(peek(0)))
            {
                runtimeError("Operand must be a number or complex number.");
                return Result::RUNTIME_ERROR;
            }

            Value operand = peek(0);
            if(is<ObjComplex>(operand))
            {
                const auto result = -as<ObjComplex>(operand)->c;
                auto* value = makeObj<ObjComplex>(*this,
                    result.real(), result.imag());
                pop();
                push(Value(value));
            }
            else
            {
                push(Value(-pop().as_number()));
            }
            break;
        }
        case OpCode::IS_ERROR:
        {
            const Value value = peek(0);
            const bool flag = is<ObjError>(value) && as<ObjError>(value)->kind != ErrorKind::CRITICAL_ERROR;
            push(Value(flag));
            break;
        }
        case OpCode::RETURN:
        {
            Value result = pop();
            closeUpvalues(frame->slots);
            stackTop = frame->slots;
            frameCount--;

            if(frameCount == 0)
                return Result::OK;

            push(result);
            frame = &frames[frameCount-1];
            break;
        }
        case OpCode::FAIL:
        {
            Value payload = pop();
            Value messageValue = pop();
            Value kindValue = pop();

            if(rejectError(kindValue, "Can't use an error value as an error kind.") ||
                rejectError(messageValue, "Can't use an error value as an error message."))
                return Result::RUNTIME_ERROR;

            ErrorKind kind;
            if(!decodeErrorKind(kindValue, kind))
            {
                runtimeError("Error kind must be one of the built-in error kind constants.");
                return Result::RUNTIME_ERROR;
            }
            if(!is<ObjString>(messageValue))
            {
                runtimeError("Error message must be a string.");
                return Result::RUNTIME_ERROR;
            }

            const std::string message = as_string(messageValue);
            Value result = makeErrorResult(kind, message, payload);
            closeUpvalues(frame->slots);
            stackTop = frame->slots;
            frameCount--;

            if(frameCount == 0)
            {
                const auto* error = as<ObjError>(result);
                const auto kindName = errorKindName(error->kind);
                const auto& errorMessage = error->message->str();
                runtimeError("Unhandled error result. Original error [{}]: {}",
                    kindName, errorMessage);
                return Result::RUNTIME_ERROR;
            }

            push(result);
            frame = &frames[frameCount-1];
            break;
        }
        case OpCode::PRINT:
        {
            printValue(pop());
            printf("\n");
            break;
        }
        case OpCode::DEFINE_GLOBAL:
        {
            ObjString* name = read_str(frame);
            globals.set(name, peek(0));
            pop();
            break;
        }
        case OpCode::GET_GLOBAL:
        {
            ObjString* name = read_str(frame);
            Value value;
            if(!globals.get(name, value))
            {
                runtimeError("Undefined variable '{}'.", name->str());
                return Result::RUNTIME_ERROR;
            }

            push(value);
            break;
        }
        case OpCode::SET_GLOBAL:
        {
            ObjString* name = read_str(frame);
            if(globals.set(name, peek(0)))
            {
                globals.del(name);
                runtimeError("Undefined variable '{}'.", name->str());
                return Result::RUNTIME_ERROR;
            }
            break;
        }
        case OpCode::GET_LOCAL:
        {
            u8 slot = read_byte(frame);
            push(frame->slots[slot]);
            break;
        }
        case OpCode::SET_LOCAL:
        {
            u8 slot = read_byte(frame);
            frame->slots[slot] = peek(0);
            break;
        }
        case OpCode::JUMP:
        {
            u16 offset = read_short(frame);
            frame->ip += offset;
            break;
        }
        case OpCode::JUMP_IF_FALSE:
        {
            u16 offset = read_short(frame);
            if(isFalsy(peek(0)))
                frame->ip += offset;
            break;
        }
        case OpCode::LOOP:
        {
            u16 offset = read_short(frame);
            frame->ip -= offset;
            break;
        }
        case OpCode::CALL:
        {
            int argCount = read_byte(frame);
            if(!callValue(peek(argCount), argCount))
                return Result::RUNTIME_ERROR;
            frame = &frames[frameCount-1];
            break;
        }
        case OpCode::CLOSURE:
        {
            auto func = as<ObjFunction>(read_constant(frame));
            auto clos = makeObj<ObjClosure>(*this, func);
            push(Value(clos));

            for(int i=0; i<clos->upvalueCount; i++)
            {
                u8 isLocal = read_byte(frame);
                u8 index = read_byte(frame);

                if(isLocal)
                {
                    clos->upvalues[i] = captureUpvalue(frame->slots+index);
                }
                else
                {
                    clos->upvalues[i] = frame->clos->upvalues[index];
                }
            }

            break;
        }
        case OpCode::GET_UPVALUE:
        {
            u8 slot = read_byte(frame);
            push(*frame->clos->upvalues[slot]->location);
            break;
        }
        case OpCode::SET_UPVALUE:
        {
            u8 slot = read_byte(frame);
            *frame->clos->upvalues[slot]->location = peek(0);
            break;
        }
        case OpCode::CLOSE_UPVALUE:
        {
            closeUpvalues(stackTop - 1);
            pop();
            break;
        }
        case OpCode::CLASS:
            push(Value(makeObj<ObjClass>(*this, read_str(frame))));
            break;
        case OpCode::GET_PROPERTY:
        {
            Value receiver = peek(0);
            ObjString* name = read_str(frame);
            if(rejectError(receiver, "Can't access a property on an error value."))
                return Result::RUNTIME_ERROR;
            if(is<ObjInstance>(receiver) || is<ObjClass>(receiver))
            {
                if(findNativeProperty(objType(receiver), name->str()) != nullptr)
                {
                    if(!getNativeProperty(receiver, name))
                        return Result::RUNTIME_ERROR;
                    break;
                }
            }
            if(is<ObjInstance>(receiver))
            {
                if(!getInstanceProperty(as<ObjInstance>(receiver), name))
                    return Result::RUNTIME_ERROR;
                break;
            }
            if(is<ObjClass>(receiver))
            {
                if(!getClassProperty(as<ObjClass>(receiver), name))
                    return Result::RUNTIME_ERROR;
                break;
            }
            if(receiver.is_obj() && hasNativeType(objType(receiver)))
            {
                if(!getNativeProperty(receiver, name))
                    return Result::RUNTIME_ERROR;
                break;
            }

            runtimeError("Only instances and classes have properties.");
            return Result::RUNTIME_ERROR;
        }
        case OpCode::SET_PROPERTY:    
        {
            Value receiver = peek(1);
            ObjString* name = read_str(frame);
            if(rejectError(receiver, "Can't assign a property on an error value."))
                return Result::RUNTIME_ERROR;
            if((is<ObjInstance>(receiver) || is<ObjClass>(receiver)) &&
                findNativeProperty(objType(receiver), name->str()) != nullptr)
            {
                if(!setNativeProperty(receiver, name))
                    return Result::RUNTIME_ERROR;
                break;
            }
            if(is<ObjInstance>(receiver))
            {
                auto instance = as<ObjInstance>(receiver);
                instance->fields.set(name, peek(0));
                Value value = pop();
                pop();
                push(value);
                break;
            }
            if(is<ObjClass>(receiver))
            {
                if(!setClassProperty(as<ObjClass>(receiver), name, peek(0)))
                    return Result::RUNTIME_ERROR;
                Value value = pop();
                pop();
                push(value);
                break;
            }
            if(receiver.is_obj() && hasNativeType(objType(receiver)))
            {
                if(!setNativeProperty(receiver, name))
                    return Result::RUNTIME_ERROR;
                break;
            }

            runtimeError("Only instances and classes have fields.");
            return Result::RUNTIME_ERROR;
        }

        case OpCode::METHOD:
            defineMethod(read_str(frame), false);
            break;
        case OpCode::STATIC_METHOD:
            defineMethod(read_str(frame), true);
            break;
        
        case OpCode::INVOKE:
        {
            ObjString* meth = read_str(frame);
            int argCount = read_byte(frame);
            if(!invoke(meth, argCount))
                return Result::RUNTIME_ERROR;
            frame = &frames[frameCount-1];
            break;
        }

        case OpCode::INHERIT:
        {
            Value superclass = peek(1);

            if(!is<ObjClass>(superclass))
            {
                runtimeError("Superclass must be a class.");
                return Result::RUNTIME_ERROR;
            }

            auto subclass = as<ObjClass>(peek(0));
            subclass->superclass = as<ObjClass>(superclass);

            pop();
            break;
        }

        case OpCode::GET_SUPER:
        {
            auto name = read_str(frame);
            auto superclass = as<ObjClass>(pop());

            if(!bindSuperMethod(superclass, name))
                return Result::RUNTIME_ERROR;
            break;
        }

        case OpCode::SUPER_INVOKE:
        {
            auto method = read_str(frame);
            int argCount = read_byte(frame);
            auto superclass = as<ObjClass>(pop());
            if(!invokeSuper(superclass, method, argCount))
                return Result::RUNTIME_ERROR;
            frame = &frames[frameCount-1];
            break;
        }

        case OpCode::MAKE_ARRAY:
        {
            auto count = read_byte(frame);
            auto array = makeObj<ObjArray>(*this);
            TempRootGuard root(*this, array);

            try
            {
                array->elements.reserve(count);
                Value* first = stackTop - count;
                for (size_t i = 0; i < count; i++)
                    array->elements.push_back(first[i]);
            }
            catch(const std::bad_alloc&)
            {
                Value err = makeErrorResult(ErrorKind::CRITICAL_ERROR, "OOM: Not enough memory to make array.", Value());
                push(err);
                break;
            }
            catch(const std::length_error&)
            {
                Value err = makeErrorResult(ErrorKind::CRITICAL_ERROR, "OOM: Array too large.", Value());
                push(err);
                break;
            }

            stackTop -= count;
            push(Value(array));
            break;
        }

        case OpCode::ITER_SNAP:
        {
            const u8 width = read_byte(frame);
            Value iterable = peek(0);

            if(width != 1 && width != 2)
            {
                runtimeError("Invalid range width {}.", width);
                return Result::RUNTIME_ERROR;
            }

            if(!is<ObjArray>(iterable) && !is<ObjMap>(iterable))
            {
                runtimeError("Range expects an array or map.");
                return Result::RUNTIME_ERROR;
            }

            try
            {
                auto* snapshot = makeObj<ObjArray>(*this);
                // Keep both the source and the in-progress snapshot rooted.
                push(Value(snapshot));

                if(is<ObjArray>(iterable))
                {
                    const auto* array = as<ObjArray>(iterable);
                    if(array->len() >
                        std::numeric_limits<size_t>::max() / width)
                    {
                        throw std::length_error("Range snapshot is too large.");
                    }

                    snapshot->elements.reserve(array->len() * width);
                    for(size_t i = 0; i < array->len(); ++i)
                    {
                        if(width == 2)
                        {
                            snapshot->elements.push_back(
                                Value(static_cast<double>(i)));
                        }
                        snapshot->elements.push_back(array->elements[i]);
                    }
                }
                else
                {
                    const auto* map = as<ObjMap>(iterable);
                    if(map->len() >
                        std::numeric_limits<size_t>::max() / width)
                    {
                        throw std::length_error("Range snapshot is too large.");
                    }

                    snapshot->elements.reserve(map->len() * width);
                    map->vt.foreach(
                        [snapshot, width](const Value& key, const Value& value) {
                            snapshot->elements.push_back(key);
                            if(width == 2)
                                snapshot->elements.push_back(value);
                        });
                }

                Value completed = pop();
                pop();
                push(completed);
            }
            catch(const std::bad_alloc&)
            {
                runtimeError("Not enough memory to create range snapshot.");
                return Result::RUNTIME_ERROR;
            }
            catch(const std::length_error&)
            {
                runtimeError("Collection is too large to iterate.");
                return Result::RUNTIME_ERROR;
            }
            break;
        }

        case OpCode::GET_INDEX:
        {
            Value index = peek(0);
            Value receiver = peek(1);
            Value e;
            
            if(is<ObjArray>(receiver))
            {
                auto array = as<ObjArray>(receiver);
                auto len = array->len();
                auto res = normalIndex(index, len);

                if(res.result!=IndexResult::OK)
                {
                    e = makeIndexError(index, len, res.result);
                    goto get_finally;
                }

                e = array->elements[res.index];
            }
            else if(is<ObjMap>(receiver))
            {
                auto map = as<ObjMap>(receiver);
                try
                {
                    if(!map->vt.get(index, e))
                        e = Value();
                }
                catch(const std::invalid_argument& error)
                {
                    const ErrorKind kind = validateKey(index) == HashResult::NAN_VALUE
                        ? ErrorKind::VALUE_ERROR
                        : ErrorKind::TYPE_ERROR;
                    e = makeErrorResult(kind, error.what(), index);
                }
            }
            else
            {
                e = makeErrorResult(ErrorKind::TYPE_ERROR, "Only arrays and maps can be indexed.", receiver);
            }

            get_finally:
            pop();
            pop();
            push(e);
            break;
        }

        case OpCode::SET_INDEX:
        {
            Value val = peek(0);
            Value index = peek(1);
            Value receiver = peek(2);
            Value e;

            if (is<ObjArray>(receiver))
            {
                auto array = as<ObjArray>(receiver);
                auto res = normalIndex(index, array->len());

                if(res.result!=IndexResult::OK)
                {
                    e = makeIndexError(index, array->len(), res.result);
                    goto set_finally;
                }
                array->elements[res.index] = val;
            }
            else if(is<ObjMap>(receiver))
            {
                auto map = as<ObjMap>(receiver);
                try
                {
                    map->vt.set(index, val);
                    e = val;
                }
                catch(const std::invalid_argument& error)
                {
                    const ErrorKind kind = validateKey(index) == HashResult::NAN_VALUE
                        ? ErrorKind::VALUE_ERROR
                        : ErrorKind::TYPE_ERROR;
                    e = makeErrorResult(kind, error.what(), index);
                }
                catch(const std::bad_alloc&)
                {
                    e = makeErrorResult(ErrorKind::CRITICAL_ERROR,
                        "Not enough memory to grow map.", receiver);
                }
                catch(const std::length_error&)
                {
                    e = makeErrorResult(ErrorKind::CRITICAL_ERROR,
                        "Map is too large to grow.", receiver);
                }
            }
            else
            {
                e = makeErrorResult(ErrorKind::TYPE_ERROR, "Only arrays and maps can be indexed.", receiver);
            }

            set_finally:
            pop();
            pop();
            pop();
            push(e);
            break;
        }

        case OpCode::EXTEND:
        {
            Value impl = peek(0);
            Value methodName = peek(1);
            Value typeName = peek(2);

            if(!is<ObjString>(typeName) || !is<ObjString>(methodName))
            {
                runtimeError("Extension type and method names must be strings.");
                return Result::RUNTIME_ERROR;
            }
            if(!is<ObjClosure>(impl))
            {
                runtimeError("Extension impl must be a script function.");
                return Result::RUNTIME_ERROR;
            }

            const auto& type = as<ObjString>(typeName)->str();
            const auto& method = as<ObjString>(methodName)->str();
            Table* extensions = extensionTable(type);
            if(extensions == nullptr)
            {
                runtimeError("Type '{}' does not support extension methods.", type);
                return Result::RUNTIME_ERROR;
            }
            if(as<ObjClosure>(impl)->func->arity < 1)
            {
                runtimeError(
                    "Extension method '{}.{}' must accept its receiver as the first parameter.",
                    type, method);
                return Result::RUNTIME_ERROR;
            }
            if(nativeMethodExistsForExtensionType(type, method))
            {
                runtimeError("Native method '{}.{}' cannot be replaced.", type, method);
                return Result::RUNTIME_ERROR;
            }

            Value existing;
            auto* methodString = as<ObjString>(methodName);
            if(extensions->get(methodString, existing))
            {
                runtimeError("Extension method '{}.{}' is already registered.", type, method);
                return Result::RUNTIME_ERROR;
            }

            extensions->set(methodString, impl);
            pop();
            pop();
            pop();

            break;
        }

        default:
            runtimeError("Unknown opcode {}.", instruction);
            return Result::RUNTIME_ERROR;
        }
    }
}

void VM::concat()
{
    auto* b = as<ObjString>(peek(0));
    auto* a = as<ObjString>(peek(1));
    auto* result = copyString(*this, a->str() + b->str());

    pop();
    pop();
    push(Value(result));
}

template<typename... Args>
void VM::runtimeError(std::string_view fmt, Args&&... args) {
    auto message = std::vformat(fmt, std::make_format_args(std::forward<Args>(args)...));

    std::cerr << "[runtime error] " << message << '\n';

    for(int i=frameCount-1; i>=0; i--)
    {
        auto frame = &frames[i];
        auto func = frame->clos->func;
        size_t instruction = frame->ip - func->chunk.code.data() - 1;
        std::cerr << "[line " << func->chunk.lines[instruction] << "] in ";
        if(func->name == nullptr)
            std::cerr << "script\n";
        else
            std::cerr << func->name->str() << "()\n";
    }

    resetStack();
}

Value VM::makeErrorResult(ErrorKind kind, std::string_view message, Value payload)
{
    const bool payloadRooted = payload.is_obj();
    if(payloadRooted)
        push(payload);

    ObjString* msg = copyString(*this, message);
    push(Value(msg));

    ObjError* result = nullptr;
    try
    {
        result = makeObj<ObjError>(*this, kind, msg, payload);
    }
    catch(...)
    {
        pop();
        if(payloadRooted)
            pop();
        throw;
    }

    pop();
    if(payloadRooted)
        pop();
    return Value(result);
}

bool VM::rejectError(Value value, std::string_view context)
{
    if(!is<ObjError>(value))
        return false;

    auto* error = as<ObjError>(value);
    const auto kind = errorKindName(error->kind);
    const auto& message = error->message->str();
    runtimeError("{} Original error [{}]: {}", context, kind, message);
    return true;
}

Value VM::makeIndexError(Value index, size_t length, IndexResult reason)
{
    switch (reason)
    {
    case IndexResult::NOT_NUMBER:
        return makeErrorResult(ErrorKind::INDEX_ERROR, "Array index must be an integral number.", index);
    case IndexResult::NOT_FINITE:
        return makeErrorResult(ErrorKind::INDEX_ERROR, "Array index must be finite.", index);
    case IndexResult::NOT_INTEGRAL:
        return makeErrorResult(ErrorKind::INDEX_ERROR, "Array index must be integral.", index);
    case IndexResult::NOT_SAFE:
        return makeErrorResult(ErrorKind::INDEX_ERROR, "Array index is outside the integer range.", index);
    case IndexResult::OUT_OF_RANGE:
        return makeErrorResult(ErrorKind::INDEX_ERROR, std::format("Array index out of range for length {}.", length), index);
    case IndexResult::END:
        return makeErrorResult(ErrorKind::INDEX_ERROR, std::format("Array index {} points past the end of length {}.", length, length), index);
    case IndexResult::OK:
    default:
        return Value();
    }
}

void VM::freeObjs()
{
    #ifdef DEBUG_LOG_GC
    utils::TerminalColor<utils::Color::Cyan> color;
    #endif
    
    while(objects != nullptr)
    {
        Obj* object = objects;
        objects = object->next;
        freeObj(object);
    }
}

void VM::freeObj(Obj* object)
{
    #ifdef DEBUG_LOG_GC
    printf("%p free type %d\n", (void*)object, static_cast<int>(object->type));
    #endif

    VM* owner = object->owner;
    size_t size = saturatingAdd(sizeof(Obj), objectExtraBytes(object));
    switch(object->type)
    {
    case ObjType::CLASS:
        size = saturatingAdd(sizeof(ObjClass), objectExtraBytes(object));
        delete as<ObjClass>(object);
        break;
    case ObjType::ERROR:
        size = sizeof(ObjError);
        delete as<ObjError>(object);
        break;
    case ObjType::INSTANCE:
        size = sizeof(ObjInstance);
        delete as<ObjInstance>(object);
        break;
    case ObjType::STRING:
        size = saturatingAdd(sizeof(ObjString), objectExtraBytes(object));
        delete as<ObjString>(object);
        break;
    case ObjType::FUNCTION:  
        size = saturatingAdd(sizeof(ObjFunction), objectExtraBytes(object));
        delete as<ObjFunction>(object);
        break;
    case ObjType::NATIVE:
        size = sizeof(ObjNative);
        delete as<ObjNative>(object);
        break;
    case ObjType::CLOSURE:
        size = sizeof(ObjClosure);
        delete as<ObjClosure>(object);
        break;
    case ObjType::UPVALUE:
        size = sizeof(ObjUpvalue);
        delete as<ObjUpvalue>(object);
        break;
    case ObjType::BOUND_METHOD:
        size = sizeof(ObjBoundMethod);
        delete as<ObjBoundMethod>(object);
        break;
    case ObjType::COMPLEX:
        size = sizeof(ObjComplex);
        delete as<ObjComplex>(object);
        break;
    case ObjType::ARRAY:
        size = sizeof(ObjArray);
        delete as<ObjArray>(object);
        break;
    case ObjType::MAP:
        size = sizeof(ObjMap);
        delete as<ObjMap>(object);
        break;
    case ObjType::FILE:
        size = sizeof(ObjFile);
        delete as<ObjFile>(object);
        break;
    default:
        delete object;
        break;
    }

    trackAlloc(owner, size, 0);
}

template<typename Op>
bool VM::binaryOp(Op op)
{
    if(rejectError(peek(0), "Can't use an error value in arithmetic.") ||
       rejectError(peek(1), "Can't use an error value in arithmetic."))
        return false;

    if(!peek(0).is_number() || !peek(1).is_number()) {
        runtimeError("Operands must be real numbers.");
        return false;
    }
    
    double b = pop().as_number();
    double a = pop().as_number();
    push(Value(op(a,b))); 
    return true;
}

template<typename Op>
bool VM::complexBinaryOp(Op op)
{
    const Value right = peek(0);
    const Value left = peek(1);

    if(rejectError(right, "Can't use an error value in arithmetic.") ||
       rejectError(left, "Can't use an error value in arithmetic."))
        return false;

    if(!isNumeric(left) || !isNumeric(right))
    {
        runtimeError("Operands must be numbers or complex numbers.");
        return false;
    }

    const auto result = op(asComplexNumber(left), asComplexNumber(right));

    if(is<ObjComplex>(left) || is<ObjComplex>(right))
    {
        auto* value = makeObj<ObjComplex>(*this, result.real(), result.imag());
        pop();
        pop();
        push(Value(value));
    }
    else
    {
        pop();
        pop();
        push(Value(result.real()));
    }
    return true;
}

bool VM::callValue(Value callee, int argCount)
{
    if(rejectError(callee, "Can't call an error value."))
        return false;

    if(callee.is_obj())
    {
        switch (objType(callee))
        {
        case ObjType::FUNCTION:
            return call(as<ObjClosure>(callee), argCount);
            break;
        case ObjType::NATIVE:
        {
            auto native = as<ObjNative>(callee);
            if(native->arity >= 0 && argCount != native->arity)
            {
                runtimeError("Expected {} arguments but got {}.", native->arity, argCount);
                return false;
            }

            NativeResult result = native->func(
                *this,
                argCount,
                stackTop - argCount);
            stackTop -= argCount + 1;
            push(result.ok
                ? result.value
                : makeErrorResult(result.error.kind, result.error.message, result.error.payload));
            return true;
        }
        case ObjType::CLOSURE:
            return call(as<ObjClosure>(callee), argCount);
        case ObjType::CLASS:
        {
            auto klass = as<ObjClass>(callee);
            stackTop[-argCount-1] =
                Value(makeObj<ObjInstance>(*this, klass));

            ClassMember init;
            if(findMember(klass, initStr, init) && !init.isStatic)
            {
                if(!is<ObjClosure>(init.value))
                {
                    runtimeError("Initializer must be a method.");
                    return false;
                }
                return call(as<ObjClosure>(init.value), argCount);
            }
            else if(argCount !=0)
            {
                runtimeError("Expected 0 arguments but got {}.", argCount);
                return false;
            }

            return true;
        }
        case ObjType::BOUND_METHOD:
        {
            auto bound = as<ObjBoundMethod>(callee);
            stackTop[-argCount-1] = bound->receiver;
            return call(bound->method, argCount);
        }
        default:
            // unreachable
            break;
        }
    }

    runtimeError("Can only call functions and classes.");
    return false;
}

bool VM::call(ObjClosure* clos, int argCount)
{
    ObjFunction* function = clos->func;
    if(!goodArity(function, argCount))
    {
        if(function->variadic)
            runtimeError("Expected at least {} arguments but got {}.", function->arity, argCount);
        else
            runtimeError("Expected {} arguments but got {}.", function->arity, argCount);
        return false;
    }

    if(frameCount == FRAMES_MAX)
    {
        runtimeError("Stack overflow.");
        return false;
    }

    if(function->variadic)
    {
        Value* slots = stackTop - argCount - 1;
        const int extraCount = argCount - function->arity;
        ObjArray* rest = nullptr;

        try
        {
            rest = makeObj<ObjArray>(*this);
            TempRootGuard root(*this, rest);
            rest->elements.reserve(static_cast<size_t>(extraCount));

            Value* firstExtra = slots + function->arity + 1;
            for(int i = 0; i < extraCount; ++i)
                rest->elements.push_back(firstExtra[i]);

            // Drop the separate extra arguments and replace them with the
            // single array occupying the rest parameter's local slot.
            stackTop = slots + function->arity + 1;
            push(Value(rest));
        }
        catch(const std::bad_alloc&)
        {
            runtimeError("Not enough memory to collect variadic arguments.");
            return false;
        }
        catch(const std::length_error&)
        {
            runtimeError("Too many variadic arguments to collect.");
            return false;
        }

        argCount = function->arity + 1;
    }

    Value* slots = stackTop - argCount - 1;
    frames[frameCount++] = {clos, function->chunk.code.data(), slots};
    return true;
}

void VM::defineNative(const NativeDef& def)
{
    ObjString* name = copyString(*this, def.name);
    push(Value(name));

    bool nativeRooted = false;
    try
    {
        ObjNative* native = makeObj<ObjNative>(*this, def.function, def.arity);
        push(Value(native));
        nativeRooted = true;
        globals.set(name, peek(0));
    }
    catch(...)
    {
        if(nativeRooted)
            pop();
        pop();
        throw;
    }

    pop();
    pop();
}

ObjUpvalue* VM::captureUpvalue(Value* local)
{
    ObjUpvalue* prev = nullptr;
    ObjUpvalue* upv = openUpvalues;
    while(upv!=nullptr && upv->location>local)
    {
        prev = upv;
        upv = upv->next;
    }
    if(upv!=nullptr && upv->location==local)
        return upv;

    auto created = makeObj<ObjUpvalue>(*this, local);
    created->next = upv;

    if(prev==nullptr)
        openUpvalues = created;
    else
        prev->next = created;

    return created;
}

void VM::closeUpvalues(Value* last)
{
    while(openUpvalues!=nullptr && openUpvalues->location>=last)
    {
        auto upv = openUpvalues;
        upv->closed = *upv->location;
        upv->location = &upv->closed;
        openUpvalues = upv->next;
    }
}

bool VM::findMember(ObjClass* klass, ObjString* name, ClassMember& member)
{
    for(ObjClass* current = klass; current != nullptr; current = current->superclass)
    {
        if(current->members.get(name, member))
            return true;
    }
    return false;
}

bool VM::getClassProperty(ObjClass* klass, ObjString* name)
{
    ClassMember member;
    if(!findMember(klass, name, member))
    {
        runtimeError("Undefined property '{}'.", name->str());
        return false;
    }
    if(!member.isStatic)
    {
        runtimeError("Only static properties can be accessed on classes.");
        return false;
    }

    pop();
    push(member.value);
    return true;
}

bool VM::getInstanceProperty(ObjInstance* instance, ObjString* name)
{
    Value value;
    if(instance->fields.get(name, value))
    {
        pop();
        push(value);
        return true;
    }

    return bindMethod(instance->klass, name);
}

bool VM::setClassProperty(ObjClass* klass, ObjString* name, Value value)
{
    ClassMember member{value, false};
    if(!klass->members.get(name, member))
    {
        ClassMember inherited;
        if(findMember(klass->superclass, name, inherited))
            member.isStatic = inherited.isStatic;
    }
    else
    {
        member.value = value;
    }

    klass->members.set(name, member);
    return true;
}

bool VM::bindMethod(ObjClass* klass, ObjString* name)
{
    ClassMember member;
    if(!findMember(klass, name, member))
    {
        runtimeError("Undefined property '{}'.", name->str());
        return false;
    }

    if(member.isStatic || !is<ObjClosure>(member.value))
    {
        pop();
        push(member.value);
        return true;
    }

    auto bound = makeObj<ObjBoundMethod>(*this, peek(0), as<ObjClosure>(member.value));
    pop();
    push(Value(bound));
    return true;
}

bool VM::bindSuperMethod(ObjClass* klass, ObjString* name)
{
    ClassMember member;
    if(!findMember(klass, name, member))
    {
        runtimeError("Undefined property '{}'.", name->str());
        return false;
    }
    if(member.isStatic || !is<ObjClosure>(member.value))
    {
        runtimeError("Superclass member '{}' is not an instance method.", name->str());
        return false;
    }

    auto bound = makeObj<ObjBoundMethod>(*this, peek(0), as<ObjClosure>(member.value));
    pop();
    push(Value(bound));
    return true;
}

bool VM::invoke(ObjString* name, int argCount)
{
    Value receiver = peek(argCount);
    if(rejectError(receiver, "Can't invoke a method on an error value."))
        return false;

    if(is<ObjInstance>(receiver))
    {
        auto instance = as<ObjInstance>(receiver);

        Value value;
        if(instance->fields.get(name, value))
        {
            stackTop[-argCount-1] = value;
            return callValue(value, argCount);
        }

        if(findNativeMethod(ObjType::INSTANCE, name->str()) != nullptr)
            return invokeNativeMethod(receiver, name, argCount);

        return invokeClass(instance->klass, name, argCount);
    }

    if(is<ObjClass>(receiver))
    {
        if(findNativeMethod(ObjType::CLASS, name->str()) != nullptr)
            return invokeNativeMethod(receiver, name, argCount);

        ClassMember member;
        if(!findMember(as<ObjClass>(receiver), name, member))
        {
            runtimeError("Undefined property '{}'.", name->str());
            return false;
        }
        if(!member.isStatic)
        {
            runtimeError("Only static methods can be called on classes.");
            return false;
        }

        stackTop[-argCount-1] = member.value;
        return callValue(member.value, argCount);
    }

    if(receiver.is_obj() && hasNativeType(objType(receiver)))
        return invokeNativeMethod(receiver, name, argCount);

    runtimeError("Only instances and classes have methods.");
    return false;
}

bool VM::getNativeProperty(Value receiver, ObjString* name)
{
    const auto* property = findNativeProperty(
        objType(receiver), name->str());

    NativeResult result = property != nullptr
        ? property->getter(*this, receiver)
        : NativeResult::failure(
            ErrorKind::NAME_ERROR,
            std::format(
                "Native value has no readable attribute '{}'.",
                name->str()),
            receiver);

    Value output = result.ok
        ? result.value
        : makeErrorResult(
            result.error.kind,
            result.error.message,
            result.error.payload);

    pop();
    push(output);
    return true;
}

bool VM::setNativeProperty(Value receiver, ObjString* name)
{
    Value error = makeErrorResult(
        ErrorKind::TYPE_ERROR,
        std::format(
            "Native attribute '{}' is read-only.",
            name->str()),
        receiver);

    pop();
    pop();
    push(error);
    return true;
}

bool VM::invokeNativeMethod(Value receiver, ObjString* name, int argCount)
{
    const auto* method = findNativeMethod(
        objType(receiver), name->str());

    if(method == nullptr)
        return invokeExtensionMethod(receiver, name, argCount);

    if(method->arity >= 0 && argCount != method->arity)
    {
        Value error = makeErrorResult(
            ErrorKind::TYPE_ERROR,
            std::format(
                "Method '{}()' expects {} arguments but got {}.",
                name->str(), method->arity, argCount),
            receiver);
        stackTop -= argCount + 1;
        push(error);
        return true;
    }

    NativeResult result = method->function(
        *this,
        receiver,
        argCount,
        stackTop - argCount);

    Value output = result.ok
        ? result.value
        : makeErrorResult(
            result.error.kind,
            result.error.message,
            result.error.payload);

    stackTop -= argCount + 1;
    push(output);
    return true;
}

bool VM::invokeExtensionMethod(Value receiver, ObjString* name, int argCount)
{
    Table* extensions = extensionTable(objType(receiver));
    Value implementation;
    if(extensions == nullptr || !extensions->get(name, implementation))
    {
        Value error = makeErrorResult(
            ErrorKind::NAME_ERROR,
            std::format(
                "Native value has no method '{}'.",
                name->str()),
            receiver);
        stackTop -= argCount + 1;
        push(error);
        return true;
    }

    if(!is<ObjClosure>(implementation))
    {
        runtimeError("Registered extension method '{}' is not a script function.", name->str());
        return false;
    }

    const auto* function = as<ObjClosure>(implementation)->func;
    const int expected = function->arity - 1;
    if(!goodArity(function, argCount + 1))
    {
        const std::string expectation = function->variadic
            ? std::format("at least {}", expected)
            : std::to_string(expected);
        Value error = makeErrorResult(
            ErrorKind::TYPE_ERROR,
            std::format(
                "Method '{}()' expects {} arguments but got {}.",
                name->str(), expectation, argCount),
            receiver);
        stackTop -= argCount + 1;
        push(error);
        return true;
    }

    // Transform [receiver, args...] into the ordinary script-call layout
    // [function, receiver, args...].
    Value* base = stackTop - argCount - 1;
    push(Value());
    for(int i = argCount; i >= 0; --i)
        base[i + 1] = base[i];
    base[0] = implementation;

    return callValue(implementation, argCount + 1);
}

bool VM::invokeClass(ObjClass* klass, ObjString* name, int argCount)
{
    ClassMember member;
    if(!findMember(klass, name, member))
    {
        runtimeError("Undefined property '{}'.", name->str());
        return false;
    }

    if(member.isStatic || !is<ObjClosure>(member.value))
    {
        stackTop[-argCount-1] = member.value;
        return callValue(member.value, argCount);
    }

    return call(as<ObjClosure>(member.value), argCount);
}

bool VM::invokeSuper(ObjClass* klass, ObjString* name, int argCount)
{
    ClassMember member;
    if(!findMember(klass, name, member))
    {
        runtimeError("Undefined property '{}'.", name->str());
        return false;
    }
    if(member.isStatic || !is<ObjClosure>(member.value))
    {
        runtimeError("Superclass member '{}' is not an instance method.", name->str());
        return false;
    }

    return call(as<ObjClosure>(member.value), argCount);
}

void VM::defineMethod(ObjString* name, bool isStatic)
{
    Value method = peek(0);
    auto klass = as<ObjClass>(peek(1));
    klass->members.set(name, ClassMember{method, isStatic});
    pop();
}

Table* VM::extensionTable(ObjType type)
{
    switch(type)
    {
    case ObjType::ARRAY:
        return &arrayExt;
    case ObjType::STRING:
        return &stringExt;
    case ObjType::MAP:
        return &mapExt;
    case ObjType::CLOSURE:
    case ObjType::NATIVE:
    case ObjType::BOUND_METHOD:
        return &functionExt;
    default:
        return nullptr;
    }
}

Table* VM::extensionTable(std::string_view name)
{
    if(name == "array")
        return &arrayExt;
    if(name == "string")
        return &stringExt;
    if(name == "map")
        return &mapExt;
    if(name == "function")
        return &functionExt;
    return nullptr;
}

bool VM::hasExtType(ObjType type) const
{
    return type == ObjType::ARRAY ||
        type == ObjType::STRING ||
        type == ObjType::MAP ||
        type == ObjType::CLOSURE ||
        type == ObjType::NATIVE ||
        type == ObjType::BOUND_METHOD;
}

bool VM::nativeMethodExistsForExtensionType(
    std::string_view typeName, std::string_view methodName) const
{
    if(typeName == "array")
        return findNativeMethod(ObjType::ARRAY, methodName) != nullptr;
    if(typeName == "string")
        return findNativeMethod(ObjType::STRING, methodName) != nullptr;
    if(typeName == "map")
        return findNativeMethod(ObjType::MAP, methodName) != nullptr;
    if(typeName == "function")
    {
        return findNativeMethod(ObjType::CLOSURE, methodName) != nullptr ||
            findNativeMethod(ObjType::NATIVE, methodName) != nullptr ||
            findNativeMethod(ObjType::BOUND_METHOD, methodName) != nullptr;
    }
    return false;
}

// ----GC----

void VM::collectGarbage()
{
    if(isCollecting)
        return;

    isCollecting = true;

    #ifdef DEBUG_LOG_GC
    utils::TerminalColor<utils::Color::Cyan> color;
    printf("-- gc begin\n");
    size_t before = bytesAlloc;
    #endif

    try
    {
        markRoots();
        traceRefs();
        removeWhite(strings);
        sweep();

        nextGC = nextCollectionThresh(bytesAlloc);
    }
    catch(...)
    {
        grayStack.clear();
        for(Obj* object = objects; object != nullptr; object = object->next)
            object->marked = false;
        isCollecting = false;
        throw;
    }

    isCollecting = false;

    #ifdef DEBUG_LOG_GC
    printf("-- gc end\n");
    const size_t collected = before > bytesAlloc ? before - bytesAlloc : 0;
    printf("collected %zu bytes (from %zu to %zu) next at %zu\n",
        collected, before, bytesAlloc, nextGC);
    #endif
}

void VM::markRoots()
{
    for(Value* slot=stack.data(); slot!=stackTop; ++slot)
        markValue(*slot);
    
    for(int i=0; i<frameCount; ++i)
        markObject(as<Obj>(frames[i].clos));

    for(auto upval = openUpvalues; upval!=nullptr; upval = upval->next)
        markObject(as<Obj>(upval));

    markObject(temporaryRoot);
    markObject(initStr);
    
    markTable(globals);
    markTable(arrayExt);
    markTable(stringExt);
    markTable(mapExt);
    markTable(functionExt);
    Compiler::comp.markCompilerRoots(*this);
}

void VM::markValue(Value value)
{
    if(value.is_obj())
        markObject(value.as_obj());
}

void VM::markObject(Obj* object)
{
    if(object==nullptr)
        return;
    if(object->marked)
        return;

    #ifdef DEBUG_LOG_GC
    utils::TerminalColor<utils::Color::Cyan> color;
    printf("%p mark ", (void*)object);
    printValue(Value(object));
    printf("\n");
    #endif

    object->marked = true;

    grayStack.push_back(object);
    if(grayStack.empty())
        throw std::runtime_error("gray stack unexpectedly empty");
}

void VM::markTable(Table& table)
{
    for(auto it=table.m.begin(); it!=table.m.end(); ++it)
    {
        markObject(it->first);
        markValue(it->second);
    }
}

void VM::markMemberTable(MemberTable& table)
{
    for(auto it=table.m.begin(); it!=table.m.end(); ++it)
    {
        markObject(it->first);
        markValue(it->second.value);
    }
}

void VM::removeWhite(StringPool& pool)
{
    for(auto it=pool.m.begin(); it!=pool.m.end();)
    {
        if(!(*it)->marked)
            it = pool.m.erase(it);
        else
            ++it;
    }
}

void VM::traceRefs()
{
    while(grayStack.size()>0)
    {
        Obj* object = grayStack.back();
        grayStack.pop_back();
        blackenObject(object);
    }
}

void VM::blackenObject(Obj* object)
{
    #ifdef DEBUG_LOG_GC
    utils::TerminalColor<utils::Color::Cyan> color;
    printf("%p blacken ", (void*)object);
    printValue(Value(object));
    printf("\n");
    #endif

    switch(object->type)
    {
    case ObjType::BOUND_METHOD:
    {
        auto bound = as<ObjBoundMethod>(object);
        markValue(bound->receiver);
        markObject(as<Obj>(bound->method));
        break;
    }
    case ObjType::CLASS:
    {
        auto klass = as<ObjClass>(object);
        markObject(klass->name);
        markObject(as<Obj>(klass->superclass));
        markMemberTable(klass->members);
        break;
    }
    case ObjType::ERROR:
    {
        auto error = as<ObjError>(object);
        markObject(error->message);
        markValue(error->payload);
        break;
    }
    case ObjType::INSTANCE:
    {
        auto inst = as<ObjInstance>(object);
        markObject(as<Obj>(inst->klass));
        markTable(inst->fields);
        break;
    }
    case ObjType::CLOSURE:
    {
        auto closure = as<ObjClosure>(object);
        markObject(as<Obj>(closure->func));
        for(auto upv: closure->upvalues)
            markObject(as<Obj>(upv));
        break;
    }
    case ObjType::FUNCTION:
    {
        auto func = as<ObjFunction>(object);
        markObject(func->name);
        markArray(func->chunk.constants);
        break;
    }
    case ObjType::UPVALUE:
        markValue(as<ObjUpvalue>(object)->closed);
        break;
    case ObjType::ARRAY:
    {
        auto arr = as<ObjArray>(object);
        for(const Value& e: arr->elements)
            markValue(e);
        break;
    }
    case ObjType::MAP:
    {
        auto map = as<ObjMap>(object);
        map->vt.foreach([this](const Value& key, const Value& value) {
            markValue(key);
            markValue(value);
        });
        break;
    }
    case ObjType::NATIVE:
    case ObjType::STRING:
    case ObjType::COMPLEX:
    case ObjType::FILE:
        break;
    case ObjType::OBJ:
    case ObjType::NONE:
        // unreachable
        break;
    }
}

void VM::markArray(ValueArray& array)
{
    for(size_t i=0; i<array.size(); i++)
        markValue(array[i]);
}

void VM::sweep()
{
    Obj* prev = nullptr;
    Obj* object = objects;
    while(object!=nullptr)
    {
        if(object->marked)
        {
            object->marked = false;
            prev = object;
            object = object->next;
        }
        else
        {
            Obj* unreached = object;
            object = object->next;

            if(prev!=nullptr)
                prev->next = object;
            else
                objects = object;

            freeObj(unreached);
        }
    }
}

// -------

void prepareAlloc(VM* owner, size_t oldSize, size_t newSize)
{
    if(owner == nullptr || newSize <= oldSize || !owner->gcEnabled || owner->isCollecting)
        return;

    const size_t growth = newSize - oldSize;
    bool shouldCollect = owner->bytesAlloc >= owner->nextGC ||
        growth > owner->nextGC - owner->bytesAlloc;
    #ifdef DEBUG_STRESS_GC
    shouldCollect = true;
    #endif

    if(shouldCollect)
        owner->collectGarbage();
}

void trackAlloc(VM* owner, size_t oldSize, size_t newSize)
{
    if(owner == nullptr || oldSize == newSize)
        return;

    if(newSize > oldSize)
    {
        owner->bytesAlloc = saturatingAdd(owner->bytesAlloc, newSize - oldSize);
    }
    else
    {
        const size_t released = oldSize - newSize;
        owner->bytesAlloc = released > owner->bytesAlloc
            ? 0
            : owner->bytesAlloc - released;
    }
}

void prepareObjAlloc(VM* owner, size_t size)
{
    prepareAlloc(owner, 0, size);
}

void allocObj(VM* owner, Obj* p, size_t size)
{
    const size_t allocationSize = saturatingAdd(size, objectExtraBytes(p));

    #ifdef DEBUG_LOG_GC
    utils::TerminalColor<utils::Color::Cyan> color;
    printf("%p allocate %zu for %d\n",
        (void*)p, allocationSize, static_cast<int>(p->type));
    #endif

    p->next = owner->objects;
    owner->objects = p;
    trackAlloc(owner, 0, allocationSize);
}

ObjString* copyString(VM& owner, std::string_view chars)
{
    if(ObjString* interned = owner.strings.find(chars); interned != nullptr)
        return interned;

    prepareAlloc(&owner, 0,
        saturatingAdd(sizeof(ObjString), saturatingAdd(chars.size(), 1)));
    auto* string = makeObj<ObjString>(owner, std::string(chars));
    Obj* previousRoot = owner.temporaryRoot;
    owner.temporaryRoot = string;
    try
    {
        owner.strings.insert(string);
    }
    catch(...)
    {
        owner.temporaryRoot = previousRoot;
        throw;
    }
    owner.temporaryRoot = previousRoot;
    return string;
}

bool goodArity(const ObjFunction* function, int supplied)
{
    return function->variadic? supplied>=function->arity: supplied==function->arity;
}
