#pragma once
#include "chunk.h"
#include "commons.h"
#include "compiler.h"
#include "object.h"
#include "debug.h"
#include "table.h"
#include "memory.h"
#include "native.h"

enum class Result
{
    OK, COMPILE_ERROR, RUNTIME_ERROR, OTHER
};

struct CallFrame 
{
    ObjClosure* clos = nullptr;
    u8* ip = nullptr;
    Value* slots = nullptr;
};

class VM
{
    friend class Compiler;
    friend void allocObj(VM*, Obj*, size_t);
    friend void prepareAllocation(VM*, size_t, size_t);
    friend void trackAllocation(VM*, size_t, size_t);
    friend ObjString* copyString(VM&, std::string_view);

    size_t bytesAlloc = 0;
    size_t nextGC = 1024*1024;
    bool gcEnabled = false;
    bool isCollecting = false;

    Vector<CallFrame> frames;
    int frameCount = 0;

    Vector<Value> stack;

    Obj* objects = nullptr;
    ObjUpvalue* openUpvalues = nullptr;
    Obj* temporaryRoot = nullptr;

    Table globals;

    Vector<Obj*> grayStack;

    VM();
    ~VM();
    VM(const VM&) = delete;
    VM& operator=(const VM&) = delete;

public:
    Table strings;
    string initStr = "init";

    static VM vm;
    
    Result interpret(const string& src);

    void push(Value val);
    Value pop();
private:
    inline u8 read_byte(CallFrame* frame)
    {
        return *frame->ip++;
    }

    inline u16 read_short(CallFrame* frame)
    {
        return (frame->ip += 2, static_cast<u16>((frame->ip[-2] << 8) | frame->ip[-1]));
    }

    inline Value read_constant(CallFrame* frame)
    {
        return frame->clos->func->chunk.constants.values[read_byte(frame)];
    }

    inline const std::string& read_str(CallFrame* frame)
    {
        return as_string(read_constant(frame));
    }

    template<typename Op>
    void binaryOp(Op op);

    Result run();

    Value peek(int dist)
    {
        if(dist < 0 || static_cast<size_t>(dist) >= stack.size())
            throw std::overflow_error("access out of bounds");
        return stack[stack.size() - 1 - static_cast<size_t>(dist)];
    }

    static bool isFalsy(Value value)
    {
        return value.is_nil() || (value.is_bool() && !value.as_bool());
    }


    template<typename... Args>
    void runtimeError(std::string_view fmt, Args&&... args);

    void resetStack();

    void concat()
    {
        auto* b = as_str(peek(0));
        auto* a = as_str(peek(1));
        auto* result = copyString(*this, a->str() + b->str());

        pop();
        pop();
        push(Value(result));
    }

    void freeObjs();
    static void freeObj(Obj* object);

    bool callValue(Value callee, int argCount);
    bool call(ObjClosure* clos, int argCount);
    void defineNative(const NativeDef& def);
    ObjUpvalue* captureUpvalue(Value* local);
    void closeUpvalues(Value* last);
    bool bindMethod(ObjClass* klass, const string& name);
    bool invoke(const string& name, int argCount);
    bool invokeClass(ObjClass* klass, const string& name, int argCount);
    void defineMethod(const string& name);

    // GC
    void collectGarbage();

    void markRoots();
    void markValue(Value value);
    void markObject(Obj* object);

    void markTable(Table& table);
    void removeWhite(Table& table);

    void traceRefs();

    void blackenObject(Obj* object);

    void markArray(ValueArray& array);
    void sweep();
};

ObjString* copyString(VM& owner, std::string_view chars);

