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

enum class IndexResult
{
    OK, 
    END,
    NOT_NUMBER,
    NOT_INTEGRAL,
    NOT_FINITE,
    NOT_SAFE,
    OUT_OF_RANGE
};

class VM
{
    friend class Compiler;
    friend class TempRootGuard;

    friend void allocObj(VM*, Obj*, size_t);
    friend void prepareAllocation(VM*, size_t, size_t);
    friend void trackAlloc(VM*, size_t, size_t);
    friend ObjString* copyString(VM&, std::string_view);

    size_t bytesAlloc = 0;
    size_t nextGC = 1024*1024;
    bool gcEnabled = false;
    bool isCollecting = false;

    std::array<CallFrame, FRAMES_MAX> frames{};
    int frameCount = 0;

    std::array<Value, STACK_MAX> stack{};
    Value* stackTop = stack.data();

    Obj* objects = nullptr;
    ObjUpvalue* openUpvalues = nullptr;
    Obj* temporaryRoot = nullptr;

    Table globals;
    StringPool strings;
    Vector<Obj*> grayStack;

    Table arrayExt;
    Table stringExt;
    Table functionExt;

public:
    VM();
    ~VM();
    VM(const VM&) = delete;
    VM& operator=(const VM&) = delete;

    ObjString* initStr = nullptr;
    
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

    inline ObjString* read_str(CallFrame* frame)
    {
        return static_cast<ObjString*>(read_constant(frame).as_obj());
    }

    template<typename Op>
    bool binaryOp(Op op);

    template<typename Op>
    bool complexBinaryOp(Op op);

    Result run();

    size_t stackSize() const;
    void resetStack();
    Value peek(int dist);

    void concat();

    template<typename... Args>
    void runtimeError(std::string_view fmt, Args&&... args);

    Value makeErrorResult(ErrorKind kind, std::string_view message, Value payload = Value());
    bool rejectError(Value value, std::string_view context);
    Value makeIndexError(Value index, size_t length, IndexResult reason);

    void freeObjs();
    static void freeObj(Obj* object);

    bool callValue(Value callee, int argCount);
    bool call(ObjClosure* clos, int argCount);
    void defineNative(const NativeDef& def);
    ObjUpvalue* captureUpvalue(Value* local);
    void closeUpvalues(Value* last);
    bool findMember(ObjClass* klass, ObjString* name, ClassMember& member);
    bool getClassProperty(ObjClass* klass, ObjString* name);
    bool getInstanceProperty(ObjInstance* instance, ObjString* name);
    bool getNativeProperty(Value receiver, ObjString* name);
    bool setNativeProperty(Value receiver, ObjString* name);
    bool setClassProperty(ObjClass* klass, ObjString* name, Value value);
    bool bindMethod(ObjClass* klass, ObjString* name);
    bool bindSuperMethod(ObjClass* klass, ObjString* name);
    bool invoke(ObjString* name, int argCount);
    bool invokeNativeMethod(Value receiver, ObjString* name, int argCount);
    bool invokeExtensionMethod(Value receiver, ObjString* name, int argCount);
    Table* extensionTable(ObjType type);
    Table* extensionTable(std::string_view typeName);
    bool hasExtensionType(ObjType type) const;
    bool nativeMethodExistsForExtensionType(
        std::string_view typeName, std::string_view methodName) const;
    bool invokeClass(ObjClass* klass, ObjString* name, int argCount);
    bool invokeSuper(ObjClass* klass, ObjString* name, int argCount);
    void defineMethod(ObjString* name, bool isStatic);

    // GC
    void collectGarbage();

    void markRoots();
    void markValue(Value value);
    void markObject(Obj* object);

    void markTable(Table& table);
    void markMemberTable(MemberTable& table);
    void removeWhite(StringPool& pool);

    void traceRefs();

    void blackenObject(Obj* object);

    void markArray(ValueArray& array);
    void sweep();
};

ObjString* copyString(VM& owner, std::string_view chars);

struct NormalIndexInfo 
{
    size_t index;
    IndexResult result;
};

NormalIndexInfo normalIndex(const Value& value, size_t length);

bool goodArity(const ObjFunction* function, int supplied);