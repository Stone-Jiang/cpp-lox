#include "vm.h"

VM VM::vm{};
Compiler Compiler::comp{};

VM::VM()
{
    frames.reserve(FRAMES_MAX);
    stack.reserve(STACK_MAX);
    defineNative("clock", clockNative);
}

VM::~VM()
{
    freeObjs();
}

Result VM::interpret(const string& src)
{
    frames.clear();
    frameCount = 0;
    stack.clear();

    

    auto* func = Compiler::comp.compile(src);
    if(func == nullptr)
        return Result::COMPILE_ERROR;

    push(Value(func));
    auto clos = new ObjClosure(func);
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
            else if(peek(0).is_number() && peek(0).is_number())
            {
                double b = pop().as_number();
                double a = pop().as_number();
                push(Value(a+b));
            }
            else
            {
                runtimeError("Operands must be two numbers or two strings.");
                return Result::RUNTIME_ERROR;
            }
            break;
        }
        
        case OpCode::SUBTRACT: binaryOp([](double a, double b) {return a-b;}); break;
        case OpCode::MULTIPLY: binaryOp([](double a, double b) {return a*b;}); break;
        case OpCode::DIVIDE:   binaryOp([](double a, double b) {return a/b;}); break;
        case OpCode::GREATER: binaryOp([](double a, double b) {return a>b;}); break;
        case OpCode::LESS: binaryOp([](double a, double b) {return a<b;}); break;

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
            if(!peek(0).is_number())
            {
                runtimeError("Operand must be a number.");
                return Result::RUNTIME_ERROR;
            }
            auto temp = -(pop().as_number());
            push(Value(temp));
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
            string name = read_str(frame);
            globals.set(name, peek(0));
            pop();
            break;
        }
        case OpCode::GET_GLOBAL:
        {
            string name = read_str(frame);
            Value value;
            if(!globals.get(name, value))
            {
                runtimeError("Undefined variable '{}'.", name);
                return Result::RUNTIME_ERROR;
            }

            push(value);
            break;
        }
        case OpCode::SET_GLOBAL:
        {
            string name = read_str(frame);
            if(globals.set(name, peek(0)))
            {
                globals.del(name);
                runtimeError("Undefined variable '{}'.", name);
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
            auto clos = new ObjClosure(func);
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
            push(Value(new ObjClass(read_str(frame))));
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
            string meth = read_str(frame);
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
            auto& to = as_class(subclass)->methods.m;
            for(auto it=from.begin(); it!=from.end();++it)
                to.insert(*it);

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
    auto message = std::vformat(fmt, std::make_format_args(std::forward<Args>(args)...));

    std::cerr << "[runtime error] " << message << '\n';

    for(int i=frameCount-1; i>=0; i--)
    {
        auto frame = &frames[i];
        auto func = frame->clos->func;
        size_t instruction = frame->ip - func->chunk.code.data() - 1;
        std::cerr << "[line " << func->chunk.lines[instruction] << "] in ";
        if(func->name.empty())
            std::cerr << "script\n";
        else
            std::cerr << func->name << "()\n";
    }

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
    printf("%p free type %d\n", (void*)object, object->type);
    #endif

    switch(object->type)
    {
    case ObjType::CLASS:
        delete static_cast<ObjClass*>(object);
        break;
    case ObjType::INSTANCE:
        delete static_cast<ObjInstance*>(object);
        break;
    case ObjType::STRING:
        delete static_cast<ObjString*>(object);
        break;
    case ObjType::FUNCTION:  
        delete static_cast<ObjFunction*>(object);
        break;
    case ObjType::NATIVE:
        delete static_cast<ObjNative*>(object);
        break;
    case ObjType::CLOSURE:
        delete static_cast<ObjClosure*>(object);
        break;
    case ObjType::UPVALUE:
        delete static_cast<ObjUpvalue*>(object);
        break;
    case ObjType::BOUND_METHOD:
        delete static_cast<ObjBoundMethod*>(object);
        break;
    default:
        delete object;
        break;
    }
}

template<typename Op>
void VM::binaryOp(Op op)
{
    if(!peek(0).is_number() || !peek(1).is_number()) {
        runtimeError("Operands must be numbers.");
        return;
    }
    
    double b = pop().as_number();
    double a = pop().as_number();
    push(Value(op(a,b))); 
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
            Value result = native(
                argCount,
                stack.data() + stack.size() - static_cast<size_t>(argCount));
            stack.resize(stack.size() - static_cast<size_t>(argCount) - 1);
            push(result);
            return true;
        }
        case ObjType::CLOSURE:
            return call(as_closure(callee), argCount);
        case ObjType::CLASS:
        {
            auto klass = as_class(callee);
            stack[stack.size()-static_cast<size_t>(argCount)-1] = Value(new ObjInstance(klass));

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

void VM::defineNative(const string& name, NativeFn func)
{
    push(Value(copyString(name)));
    push(Value(new ObjNative(func)));
    globals.set(name, stack[1]);
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

    auto created = new ObjUpvalue(local);
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

bool VM::bindMethod(ObjClass* klass, const string& name)
{
    Value method;
    if(!klass->methods.get(name, method))
    {
        runtimeError("Undefined property '{}'.", name);
        return false;
    }

    auto bound = new ObjBoundMethod(peek(0), as_closure(method));
    pop();
    push(Value(bound));
    return true;
}

bool VM::invoke(const string& name, int argCount)
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

bool VM::invokeClass(ObjClass* klass, const string& name, int argCount)
{
    Value meth;
    if(!klass->methods.get(name, meth))
    {
        runtimeError("Undefined property '{}'.", name);
        return false;
    }
    return call(as_closure(meth), argCount);
}

void VM::defineMethod(const string& name)
{
    Value method = peek(0);
    auto klass = as_class(peek(1));
    klass->methods.set(name, method);
    pop();
}

void VM::collectGarbage()
{
    #ifdef DEBUG_LOG_GC
    printf("-- gc begin\n");
    #endif

    markRoots();
    traceRefs();
    removeWhite(strings);
    sweep();

    #ifdef DEBUG_LOG_GC
    printf("-- gc end\n");
    #endif
}

void VM::markRoots()
{
    for(auto slot: stack)
        markValue(slot);
    
    for(auto fr: frames)
        markObject(static_cast<Obj*>(fr.clos));

    for(auto upval = openUpvalues; upval!=nullptr; upval->next)
        markObject(static_cast<Obj*>(upval));
    
    markTable(globals);
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
        // markObject(it->first);
        markValue(it->second);
    }
}

void VM::removeWhite(Table& table)
{
    for(auto it=table.m.begin(); it!=table.m.end();)
    {
        Obj* object = it->second.is_obj()? it->second.as_obj(): nullptr;
        if(object!=nullptr && !object->marked)
            it = table.m.erase(it);
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
        markTable(klass->methods);
        break;
    }
    case ObjType::INSTANCE:
    {
        auto inst = static_cast<ObjInstance*>(object);
        markObject(static_cast<Obj*>(inst->klass));
        markTable(inst->fields);
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
        markArray(func->chunk.constants);
        break;
    }
    case ObjType::UPVALUE:
        markValue(static_cast<ObjUpvalue*>(object)->closed);
        break;
    case ObjType::NATIVE:
    case ObjType::STRING:
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

void allocObj(Obj* p)
{
    #ifdef DEBUG_LOG_GC
    printf("%p allocate %zu for %d\n", (void*)p, sizeof(*p), p->type);
    #endif

    p->next = VM::vm.objects;
    VM::vm.objects = p;
}

ObjString* copyString(std::string_view chars)
{
    std::string key(chars);
    auto interned = VM::vm.strings.find(key);
    if(interned.has_value())
        return static_cast<ObjString*>(interned->as_obj());

    auto* string = new ObjString(std::move(key));
    VM::vm.strings.set(string->str(), Value(string));
    return string;
}

