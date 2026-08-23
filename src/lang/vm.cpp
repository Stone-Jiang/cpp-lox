#include "vm.h"
#include <algorithm>

VM VM::vm{};
Compiler Compiler::comp{};

namespace
{
bool isNumeric(const Value& value)
{
    return value.is_number() || is_complex(value);
}

std::complex<double> asComplexNumber(const Value& value)
{
    return value.is_number()
        ? std::complex<double>(value.as_number(), 0.0)
        : as_complex(value)->c;
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
        return saturatingAdd(static_cast<const ObjString*>(object)->chars.capacity(), 1);
    default:
        return 0;
    }
}

size_t nextCollectionThreshold(size_t liveBytes)
{
    const size_t max = std::numeric_limits<size_t>::max();
    const size_t grown = liveBytes > max / HEAP_GROW_FACTOR
        ? max
        : liveBytes * HEAP_GROW_FACTOR;
    return std::max(GC_MIN_THRESHOLD, grown);
}
}

VM::VM():
    frames(this), stack(this), globals(this), strings(this), grayStack(this)
{
    frames.reserve(FRAMES_MAX);
    stack.reserve(STACK_MAX);

    const auto natives = nativeDefinitions();
    globals.reserve(natives.size());

    initStr = copyString(*this, "init");

    for(const auto& nat: natives)
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
    runtimeErrorRaised = false;

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

void VM::push(Value val)
{
    if(stack.size() >= STACK_MAX)
        throw std::overflow_error("stack overflow");
    stack.push_back(val);
}

Value VM::pop()
{
    if(stack.empty())
        throw std::runtime_error("empty stack");
    auto temp = stack.back();
    stack.pop_back();
    return temp;
}

Result VM::run()
{
    auto frame = &frames[frameCount-1];

    while(true) 
    {
        #ifdef DEBUG_TRACE_EXECUTION
        printf("          ");
        for(auto it=stack.begin(); it!=stack.end(); ++it) 
        {
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
            if(is_str(peek(0)) && is_str(peek(1)))
                concat();
            else if(isNumeric(peek(0)) && isNumeric(peek(1)))
            {
                if(!complexBinaryOp([](std::complex<double> a,
                                       std::complex<double> b) { return a+b; }))
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
            if(!complexBinaryOp([](std::complex<double> a,
                                   std::complex<double> b) { return a-b; }))
                return Result::RUNTIME_ERROR;
            break;
        case OpCode::MULTIPLY:
            if(!complexBinaryOp([](std::complex<double> a,
                                   std::complex<double> b) { return a*b; }))
                return Result::RUNTIME_ERROR;
            break;
        case OpCode::DIVIDE:
            if(!complexBinaryOp([](std::complex<double> a,
                                   std::complex<double> b) { return a/b; }))
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
        case OpCode::EQUAL:
        {
            Value b = pop();
            Value a = pop();
            push(Value(a==b));
            break;
        }

        case OpCode::NEGATE:
        {
            if(!isNumeric(peek(0)))
            {
                runtimeError("Operand must be a number or complex number.");
                return Result::RUNTIME_ERROR;
            }

            Value operand = peek(0);
            if(is_complex(operand))
            {
                const auto result = -as_complex(operand)->c;
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
        case OpCode::RETURN:
        {
            Value result = pop();
            closeUpvalues(frame->slots);
            size_t slotStart = static_cast<size_t>(frame->slots - stack.data());

            frames.pop_back();
            frameCount = static_cast<int>(frames.size());
            stack.resize(slotStart);

            if(frameCount == 0)
                return Result::OK;

            push(result);
            frame = &frames.back();
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
            auto func = as_func(read_constant(frame));
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
            closeUpvalues(&stack.back());
            pop();
            break;
        }
        case OpCode::CLASS:
            push(Value(makeObj<ObjClass>(*this, read_str(frame))));
            break;
        case OpCode::GET_PROPERTY:
        {
            if(!is_instance(peek(0)))
            {
                runtimeError("Only instances have properties.");
                return Result::RUNTIME_ERROR;
            }

            auto instance = as_instance(peek(0));
            auto name = read_str(frame);
            Value value;

            if(instance->fields.get(name, value))
            {
                pop();
                push(value);
                break;
            }

            if(!bindMethod(instance->klass, name))
            {
                return Result::RUNTIME_ERROR;
            }
            break;
        }
        case OpCode::SET_PROPERTY:    
        {
            if(!is_instance(peek(1)))
            {
                runtimeError("Only instances have fields.");
                return Result::RUNTIME_ERROR;
            }

            auto instance = as_instance(peek(1));
            instance->fields.set(read_str(frame), peek(0));
            Value value = pop();
            pop();
            push(value);
            break;
        }

        case OpCode::METHOD:
            defineMethod(read_str(frame));
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

            if(!is_class(superclass))
            {
                runtimeError("Superclass must be a class.");
                return Result::RUNTIME_ERROR;
            }

            auto subclass = as_class(peek(0));

            auto& from = as_class(superclass)->methods.m;
            for(auto it=from.begin(); it!=from.end();++it)
                subclass->methods.set(it->first, it->second);

            pop();
            break;
        }

        case OpCode::GET_SUPER:
        {
            auto name = read_str(frame);
            auto superclass = as_class(pop());

            if(!bindMethod(superclass, name))
                return Result::RUNTIME_ERROR;
            break;
        }

        case OpCode::SUPER_INVOKE:
        {
            auto method = read_str(frame);
            int argCount = read_byte(frame);
            auto superclass = as_class(pop());
            if(!invokeClass(superclass, method, argCount))
                return Result::RUNTIME_ERROR;
            frame = &frames[frameCount-1];
            break;
        }

        default:
            runtimeError("Unknown opcode {}.", instruction);
            return Result::RUNTIME_ERROR;
        }
    }
}

template<typename... Args>
void VM::runtimeError(std::string_view fmt, Args&&... args) {
    runtimeErrorRaised = true;
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

void VM::reportRuntimeError(std::string_view message)
{
    runtimeError(message);
}

void VM::resetStack()
{
    closeUpvalues(stack.data());
    openUpvalues = nullptr;
    frames.clear();
    frameCount = 0;
    stack.clear();
}

void VM::freeObjs()
{
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
        delete static_cast<ObjClass*>(object);
        break;
    case ObjType::INSTANCE:
        size = sizeof(ObjInstance);
        delete static_cast<ObjInstance*>(object);
        break;
    case ObjType::STRING:
        size = saturatingAdd(sizeof(ObjString), objectExtraBytes(object));
        delete static_cast<ObjString*>(object);
        break;
    case ObjType::FUNCTION:  
        size = saturatingAdd(sizeof(ObjFunction), objectExtraBytes(object));
        delete static_cast<ObjFunction*>(object);
        break;
    case ObjType::NATIVE:
        size = sizeof(ObjNative);
        delete static_cast<ObjNative*>(object);
        break;
    case ObjType::CLOSURE:
        size = sizeof(ObjClosure);
        delete static_cast<ObjClosure*>(object);
        break;
    case ObjType::UPVALUE:
        size = sizeof(ObjUpvalue);
        delete static_cast<ObjUpvalue*>(object);
        break;
    case ObjType::BOUND_METHOD:
        size = sizeof(ObjBoundMethod);
        delete static_cast<ObjBoundMethod*>(object);
        break;
    case ObjType::COMPLEX:
        size = sizeof(ObjComplex);
        delete static_cast<ObjComplex*>(object);
        break;
    default:
        delete object;
        break;
    }

    trackAllocation(owner, size, 0);
}

template<typename Op>
bool VM::binaryOp(Op op)
{
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

    if(!isNumeric(left) || !isNumeric(right))
    {
        runtimeError("Operands must be numbers or complex numbers.");
        return false;
    }

    const auto result = op(asComplexNumber(left), asComplexNumber(right));

    if(is_complex(left) || is_complex(right))
    {
        auto* value = makeObj<ObjComplex>(*this,
            result.real(), result.imag());
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
    if(callee.is_obj())
    {
        switch (*objType(callee))
        {
        case ObjType::FUNCTION:
            return call(as_closure(callee), argCount);
            break;
        case ObjType::NATIVE:
        {
            auto native = as_native(callee);
            if(native->arity >= 0 && argCount != native->arity)
            {
                runtimeError("Expected {} arguments but got {}.", native->arity, argCount);
                return false;
            }

            Value result = native->func(
                *this,
                argCount,
                stack.data() + stack.size() - static_cast<size_t>(argCount));
            if(runtimeErrorRaised)
                return false;
            stack.resize(stack.size() - static_cast<size_t>(argCount) - 1);
            push(result);
            return true;
        }
        case ObjType::CLOSURE:
            return call(as_closure(callee), argCount);
        case ObjType::CLASS:
        {
            auto klass = as_class(callee);
            stack[stack.size()-static_cast<size_t>(argCount)-1] =
                Value(makeObj<ObjInstance>(*this, klass));

            Value init;
            if(klass->methods.get(initStr, init))
            {
                return call(as_closure(init), argCount);
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
            auto bound = as_bound_meth(callee);
            stack[stack.size()-static_cast<size_t>(argCount)-1] = bound->receiver;
            return call(bound->method, argCount);
        }
        default:
            break;
        }
    }

    runtimeError("Can only call functions and classes.");
    return false;
}

bool VM::call(ObjClosure* clos, int argCount)
{
    if(argCount!=clos->func->arity)
    {
        runtimeError("Expected {} arguments but got {}.", clos->func->arity, argCount);
        return false;
    }

    if(frameCount == FRAMES_MAX)
    {
        runtimeError("Stack overflow.");
        return false;
    }

    size_t slotStart = stack.size() - static_cast<size_t>(argCount) - 1;
    frames.push_back({clos, clos->func->chunk.code.data(), stack.data() + slotStart});
    frameCount = static_cast<int>(frames.size());
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

bool VM::bindMethod(ObjClass* klass, ObjString* name)
{
    Value method;
    if(!klass->methods.get(name, method))
    {
        runtimeError("Undefined property '{}'.", name->str());
        return false;
    }

    auto bound = makeObj<ObjBoundMethod>(*this, peek(0), as_closure(method));
    pop();
    push(Value(bound));
    return true;
}

bool VM::invoke(ObjString* name, int argCount)
{
    Value receiver = peek(argCount);

    if(!is_instance(receiver))
    {
        runtimeError("Only instances have methods.");
        return false;
    }

    auto instance = as_instance(receiver);

    Value value;
    if(instance->fields.get(name, value))
    {
        stack[stack.size()-static_cast<size_t>(argCount)-1] = value;
        return callValue(value, argCount);
    }

    return invokeClass(instance->klass, name, argCount);
}

bool VM::invokeClass(ObjClass* klass, ObjString* name, int argCount)
{
    Value meth;
    if(!klass->methods.get(name, meth))
    {
        runtimeError("Undefined property '{}'.", name->str());
        return false;
    }
    return call(as_closure(meth), argCount);
}

void VM::defineMethod(ObjString* name)
{
    Value method = peek(0);
    auto klass = as_class(peek(1));
    klass->methods.set(name, method);
    pop();
}

// ----GC----

void VM::collectGarbage()
{
    if(isCollecting)
        return;

    isCollecting = true;

    #ifdef DEBUG_LOG_GC
    printf("-- gc begin\n");
    size_t before = bytesAlloc;
    #endif

    try
    {
        markRoots();
        traceRefs();
        removeWhite(strings);
        sweep();

        nextGC = nextCollectionThreshold(bytesAlloc);
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
    for(auto slot: stack)
        markValue(slot);
    
    for(auto fr: frames)
        markObject(static_cast<Obj*>(fr.clos));

    for(auto upval = openUpvalues; upval!=nullptr; upval = upval->next)
        markObject(static_cast<Obj*>(upval));

    markObject(temporaryRoot);
    markObject(initStr);
    
    markTable(globals);
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
    printf("%p mark ", (void*)object);
    printValue(Value(object));
    printf("\n");
    #endif

    object->marked = true;

    grayStack.push_back(object);
    if(grayStack.empty())
        exit(1);
}

void VM::markTable(Table& table)
{
    for(auto it=table.m.begin(); it!=table.m.end(); ++it)
    {
        markObject(it->first);
        markValue(it->second);
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
    printf("%p blacken ", (void*)object);
    printValue(Value(object));
    printf("\n");
    #endif

    switch(object->type)
    {
    case ObjType::BOUND_METHOD:
    {
        auto bound = static_cast<ObjBoundMethod*>(object);
        markValue(bound->receiver);
        markObject(static_cast<Obj*>(bound->method));
        break;
    }
    case ObjType::CLASS:
    {
        auto klass = static_cast<ObjClass*>(object);
        markObject(klass->name);
        markTable(klass->methods);
        break;
    }
    case ObjType::INSTANCE:
    {
        auto inst = static_cast<ObjInstance*>(object);
        markObject(static_cast<Obj*>(inst->klass));
        markTable(inst->fields);
        break;
    }
    case ObjType::CLOSURE:
    {
        auto closure = static_cast<ObjClosure*>(object);
        markObject(static_cast<Obj*>(closure->func));
        for(auto upv: closure->upvalues)
            markObject(static_cast<Obj*>(upv));
        break;
    }
    case ObjType::FUNCTION:
    {
        auto func = static_cast<ObjFunction*>(object);
        markObject(func->name);
        markArray(func->chunk.constants);
        break;
    }
    case ObjType::UPVALUE:
        markValue(static_cast<ObjUpvalue*>(object)->closed);
        break;
    case ObjType::NATIVE:
    case ObjType::STRING:
    case ObjType::COMPLEX:
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

void prepareAllocation(VM* owner, size_t oldSize, size_t newSize)
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

void trackAllocation(VM* owner, size_t oldSize, size_t newSize)
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

void prepareObjAllocation(VM* owner, size_t size)
{
    prepareAllocation(owner, 0, size);
}

void allocObj(VM* owner, Obj* p, size_t size)
{
    const size_t allocationSize = saturatingAdd(size, objectExtraBytes(p));

    #ifdef DEBUG_LOG_GC
    printf("%p allocate %zu for %d\n",
        (void*)p, allocationSize, static_cast<int>(p->type));
    #endif

    p->next = owner->objects;
    owner->objects = p;
    trackAllocation(owner, 0, allocationSize);
}

ObjString* copyString(VM& owner, std::string_view chars)
{
    if(ObjString* interned = owner.strings.find(chars); interned != nullptr)
        return interned;

    prepareAllocation(&owner, 0,
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


