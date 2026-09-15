#pragma once
#include "commons.h"
#include "scanner.h"
#include "object.h"
#include "debug.h"
#include <optional>

class Compiler;
class VM;

class Parser
{
    friend class Compiler;
private:
    bool hadError = false;
    bool panic = false;
    Token cur;
    Token prev;
};

enum class Prec: u8
{
    NONE, ASSIGNMENT, 
    OR, AND, EQUALITY, COMPARISON,
    TERM, FACTOR, UNARY, CALL, PRIMARY
};

inline bool operator<=(Prec p1, Prec p2)
{
    return static_cast<int>(p1) <= static_cast<int>(p2);
}

using ParseFn = void (Compiler::*)(bool);

struct ParseRule
{
    ParseFn prefix = nullptr;
    ParseFn infix = nullptr;
    Prec prec = Prec::NONE;
    constexpr ParseRule() noexcept {}
    constexpr ParseRule(ParseFn pre, ParseFn in) noexcept: prefix(pre), infix(in), prec(Prec::NONE) {}
    constexpr ParseRule(ParseFn pre, ParseFn in, Prec p) noexcept: prefix(pre), infix(in), prec(p) {}
};

using Rules = std::array<ParseRule, static_cast<size_t>(TokenType::TEOF) + 1>;

struct RulesMaker
{
    static consteval Rules make() noexcept;
};

struct Local
{
    bool isCapt = false;
    int depth = 0; 
    Token name; 
};

struct Upvalue
{
    u8 index;
    bool isLocal;
};

enum class FunctionType: u8
{
    FUNCTION, INIT, METHOD, STATIC_METHOD, SCRIPT, LAMBDA
};

enum class MethodContext: u8
{
    NONE, INSTANCE, STATIC
};

class ClassCompiler
{
public:
    bool hasSuper = false;
    ClassCompiler* enclosing = nullptr;
};

class LoopCompiler
{
public:
    int scopeDepth = 0;
    int continueTarget = 0;
    LoopCompiler* enclosing = nullptr;
    Vector<int> breakJumps;

    explicit LoopCompiler(VM* owner = nullptr): breakJumps(owner) {}
};

class Compiler
{
    friend struct RulesMaker;
private:
    Compiler* enclosing = nullptr;
    VM* owner = nullptr;

    Parser parser;
    std::unique_ptr<Scanner> scanner = nullptr; 
    Chunk* chunk = nullptr;
    static const Rules rules;

    Local locals[UINT8_COUNT];
    int localCount = 0;
    int scopeDepth = 0;

    Upvalue upvalues[UINT8_COUNT];
    
    inline static Compiler* current = nullptr;
    ClassCompiler* currentClass = nullptr;
    LoopCompiler* currentLoop = nullptr;

    ObjFunction* func = nullptr;
    FunctionType ftype;

    Compiler(): ftype(FunctionType::SCRIPT) {}

    Compiler(VM& vm, FunctionType type);

public:
    static Compiler comp;

    ~Compiler()
    {
        if(current == this)
            current = enclosing;
    }

    Compiler(Compiler& other) = delete;
    Compiler& operator=(Compiler& other) = delete;

    ObjFunction* compile(VM& vm, const std::string& src);

    void markCompilerRoots(VM& vm);

private:
    inline void advance()
    {
        parser.prev = parser.cur;
        while(true)
        {
            parser.cur = scanner->scan();
            if(parser.cur.type != TokenType::ERROR)
                break;
            errorAtCur(parser.cur.start);
        }
    }

    inline void consume(TokenType type, const std::string& msg)
    {
        if(parser.cur.type == type)
        {
            advance();
            return;
        }
        errorAtCur(msg);
    }

    inline bool check(TokenType type)
    {
        return parser.cur.type == type;
    }

    inline bool match(TokenType type)
    {
        if(!check(type))
            return false;
        advance();
        return true;
    }

    void errorAt(Token& token, const std::string& msg);
    void error(const std::string& msg);
    void errorAtCur(const std::string& msg);

    ObjFunction* end();

    inline void emit(u8 byte)
    {
        currentChunk()->write(byte, parser.prev.line);
    }

    inline void emit(u8 byte1, u8 byte2)
    {
        emit(byte1);
        emit(byte2);
    }

    void emitReturn();
    u8 makeConstant(Value value);
    void emitConstant(Value value);
    const ParseRule& getRule(TokenType type) const noexcept;
    void parsePrec(Prec prec);
    u8 parseVar(const std::string& msg);
    u8 identConstant(Token& name);
    
    void number(bool);
    void imaginary(bool);
    void binary(bool);
    void grouping(bool);
    void unary(bool);
    void literal(bool);
    void stringy(bool);
    void else_(bool);

    void expression();
    void statement();
    void declaration();

    void varDecl();

    void variable(bool canAssign);
    void namedVariable(Token name, bool canAssign);

    void call(bool);
    void block();
    void function_(FunctionType type);
    void method(bool isStatic);
    void classDecl();
    void funDecl();

    void expressionStmt();
    void forStmt();
    void ifStmt();
    void printStmt();
    void failStmt();
    void returnStmt();
    void whileStmt();
    void breakStmt();
    void contStmt();
    void emitLoopCleanup(const LoopCompiler& loop);
    void patchBreaks(const LoopCompiler& loop);

    void beginScope();
    void endScope();

    bool identifiersEqual(Token& a, Token& b) const;

    int resolveLocal(Compiler* compiler, Token& name);
    int addUpvalue(Compiler* compiler, u8 index, bool isLocal);
    int resolveUpvalue(Compiler* compiler, Token& name);
    void addLocal(Token name);
    void declareVar();
    void markInitialized();

    void emitLoop(int loopStart);
    int emitJump(u8 instruction);
    void patchJump(int offset);

    void defineVar(u8 global);

    void and_(bool);
    void or_(bool);
    void dot(bool canAssign);
    void this_(bool);
    void super_(bool);
    MethodContext methodContext() const;
    u8 errorKindConstant(Token kind);

    u8 argumentList();
    Token syntheticToken(const char* text);
    void synchronize();

    inline Chunk* currentChunk()
    {
        return &current->func->chunk;
    }

    void arrayLit(bool);
    void subscript(bool);

    void lambda(bool);
    void parameter();
    void parameterList();
    void emitClosure(const Compiler& compiler, ObjFunction* func);

    void extendStmt();
    void rangeStmt();
};


