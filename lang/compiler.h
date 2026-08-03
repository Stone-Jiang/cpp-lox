#pragma once
#include "commons.h"
#include "scanner.h"
#include "object.h"
#include "debug.h"

class Compiler;
class VM;

class Parser
{
    friend class Compiler;
private:
    Token cur;
    Token prev;
    bool hadError = false;
    bool panic = false;
};

enum class Prec
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
    ParseRule() {}
    ParseRule(ParseFn pre, ParseFn in): prefix(pre), infix(in), prec(Prec::NONE) {}
    ParseRule(ParseFn pre, ParseFn in, Prec p): prefix(pre), infix(in), prec(p) {}
};

using Rules = std::array<ParseRule, static_cast<int>(TokenType::TEOF) + 1>;

struct RulesMaker
{
    static Rules make() noexcept;
};

struct Local
{
    Token name; 
    int depth = 0; 
    bool isCapt = false;
};

struct Upvalue
{
    u8 index;
    bool isLocal;
};

enum class FunctionType
{
    FUNCTION, INIT, METHOD, SCRIPT
};

class ClassCompiler
{
public:
    ClassCompiler* enclosing = nullptr;
    bool hasSuper = false;
};

class Compiler
{
    friend struct RulesMaker;
private:
    Compiler* enclosing = nullptr;

    Parser parser;
    std::unique_ptr<Scanner> scanner = nullptr; 
    Chunk* chunk = nullptr;
    Rules rules;

    Local locals[UINT8_COUNT];
    int localCount = 0;
    int scopeDepth = 0;

    Upvalue upvalues[UINT8_COUNT];
    
    inline static Compiler* current = nullptr;
    ClassCompiler* currentClass = nullptr;

    ObjFunction* func = nullptr;
    FunctionType ftype;

    Compiler(FunctionType type = FunctionType::SCRIPT): enclosing(current), rules(RulesMaker::make()), ftype(type)
    {
        current = this;
        func = new ObjFunction();

        if(type != FunctionType::SCRIPT && enclosing != nullptr)
            func->name.assign(enclosing->parser.prev.start,
                static_cast<size_t>(enclosing->parser.prev.len));

        Local* local = &locals[localCount++];
        local->depth = 0;
        local->isCapt = false;
        
        if(type!=FunctionType::FUNCTION)
        {
            local->name.start = "this";
            local->name.len = 4;
        }
        else
        {
            local->name.start = "";
            local->name.len = 0;
        }
    }

public:
    static Compiler comp;

    ~Compiler() = default;
    Compiler(Compiler& other) = delete;
    Compiler& operator=(Compiler& other) = delete;

    ObjFunction* compile(const std::string& src);

    void markCompilerRoots(VM& vm);

private:
    void advance();
    void consume(TokenType type, const std::string& msg);
    bool check(TokenType type);
    bool match(TokenType type);
    void errorAt(Token& token, const std::string& msg);
    void error(const std::string& msg);
    void errorAtCur(const std::string& msg);

    ObjFunction* end();
    void emit(u8 byte);
    void emit(u8 byte1, u8 byte2);

    void emitReturn();
    u8 makeConstant(Value value);
    void emitConstant(Value value);
    ParseRule& getRule(TokenType type);
    void parsePrec(Prec prec);
    u8 parseVar(const std::string& msg);
    u8 identConstant(Token& name);
    
    void number(bool);
    void binary(bool);
    void grouping(bool);
    void unary(bool);
    void literal(bool);
    void stringy(bool);

    void expression();
    void statement();
    void declaration();

    void varDecl();

    void variable(bool canAssign);
    void namedVariable(Token name, bool canAssign);

    void call(bool);
    void block();
    void function_(FunctionType type);
    void method();
    void classDecl();
    void funDecl();

    void expressionStmt();
    void forStmt();
    void ifStmt();
    void printStmt();
    void returnStmt();
    void whileStmt();

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

    u8 argumentList();
    Token syntheticToken(const string& text);
    void synchronize();

    Chunk* currentChunk()
    {
        return &current->func->chunk;
    }

};


