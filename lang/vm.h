#pragma once
#include "chunk.h"
#include "commons.h"
#include "compiler.h"
#include "object.h"
#include "debug.h"
#include "table.h"
#include "memory.h"

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
    friend void allocObj(Obj*);

    Vector<CallFrame> frames;
    int frameCount = 0;

    Vector<Value> stack;

    Obj* objects = nullptr;
    ObjUpvalue* openUpvalues = nullptr;

    Table globals;

    Vector<Obj*> grayStack;

    size_t bytesAlloc = 0;
    size_t nextGC = 1024*1024;

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

    void concat()
    {
        auto* b = as_str(pop());
        auto* a = as_str(pop());

        push(Value(copyString(a->str() + b->str())));
    }

    void freeObjs();
    static void freeObj(Obj* object);

    bool callValue(Value callee, int argCount);
    bool call(ObjClosure* clos, int argCount);
    void defineNative(const string& name, NativeFn func);
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

    // ------
    static Value clockNative(int, Value*)
    {
        return Value(static_cast<double>(clock())/CLOCKS_PER_SEC);
    }
};

void allocObj(Obj* p);
ObjString* copyString(std::string_view chars);