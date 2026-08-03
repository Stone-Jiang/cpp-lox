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

class Compiler;

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
    unique_ptr<Scanner> scanner = nullptr; 
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

    Compiler(FunctionType type = FunctionType::SCRIPT)
        : enclosing(current), rules(RulesMaker::make()), ftype(type)
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

    ObjFunction* compile(const string& src)
    {
        current = this;
        parser = Parser{};
        this->chunk = &func->chunk;
        chunk->clear();
        scanner = make_unique<Scanner>(src);
        
        advance();
        while(!match(TokenType::TEOF))
            declaration();

        auto res = end();
        return parser.hadError? nullptr: res;
    }

    void markCompilerRoots(VM& vm);

private:
    void advance()
    {
        parser.prev = parser.cur;
        while(true)
        {
            parser.cur = scanner->scan();
            if(parser.cur.type != TokenType::ERROR)
                break;
            errorAtCur(parser.cur.message.empty()? "Invalid token.": parser.cur.message);
        }
    }

    void consume(TokenType type, const string& msg)
    {
        if(parser.cur.type == type)
        {
            advance();
            return;
        }
        errorAtCur(msg);
    }

    bool check(TokenType type)
    {
        return parser.cur.type == type;
    }

    bool match(TokenType type)
    {
        if(!check(type))
            return false;
        advance();
        return true;
    }

    void errorAt(Token& token, const string& msg)
    {
        if(parser.panic)
            return;
        parser.panic = true;

        cerr<<"[compile error] line "<<token.line;

        if(token.type == TokenType::TEOF)
            cerr<<" at end";
        else if(token.type != TokenType::ERROR && token.start != nullptr && token.len > 0)
        {
            cerr<<" at '";
            cerr.write(token.start, token.len);
            cerr<<"'";
        }

        cerr<<": "<<msg<<'\n';
        parser.hadError = true;
    }

    void error(const string& msg)
    {
        errorAt(parser.prev, msg);
    }

    void errorAtCur(const string& msg)
    {
        errorAt(parser.cur, msg);
    }

    ObjFunction* end()
    {
        emitReturn();
        auto func = current->func;
        
        #ifdef DEBUG_PRINT_CODE
        if(!parser.hadError)
            Debug::disassembleChunk(*currentChunk(), !func->name.empty()? func->name: "<script>");
        #endif

        current = current->enclosing;
        return func;
    }

    void emit(u8 byte)
    {
        currentChunk()->write(byte, parser.prev.line);
    }

    void emit(u8 byte1, u8 byte2)
    {
        emit(byte1);
        emit(byte2);
    }

    void number(bool)
    {
        double val = strtod(parser.prev.start, nullptr);
        emitConstant(Value(val));
    }

    void emitReturn()
    {
        if(current->ftype==FunctionType::INIT)
            emit(OpCode::GET_LOCAL, 0);
        else
            emit(OpCode::NIL);
        emit(OpCode::RETURN);
    }

    u8 makeConstant(Value value)
    {
        int constant = currentChunk()->addConst(value);
        if(constant < 0 || constant > UINT8_MAX)
        {
            error("Too many constants in one chunk.");
            return 0;
        }
        return static_cast<u8>(constant);
    }

    void emitConstant(Value value)
    {
        emit(OpCode::CONSTANT, makeConstant(value));
    }

    void expression()
    {
        parsePrec(Prec::ASSIGNMENT);
    }

    void statement()
    {
        if(match(TokenType::PRINT))
        {
            printStmt();
        }
        else if(match(TokenType::FOR))
        {
            forStmt();
        }
        else if(match(TokenType::IF))
        {
            ifStmt();
        }
        else if(match(TokenType::RETURN))
        {
            returnStmt();
        }
        else if(match(TokenType::WHILE))
        {
            whileStmt();
        }
        else if(match(TokenType::LEFT_BRACE)) 
        {
            beginScope();
            block();
            endScope();
        }
        else
        {
            expressionStmt();
        }
    }

    void declaration()
    {
        if(match(TokenType::CLASS))
            classDecl();
        else if(match(TokenType::FUN))
            funDecl();
        else if(match(TokenType::VAR))
            varDecl();
        else
            statement();

        if(parser.panic)
            synchronize();
    }

    ParseRule& getRule(TokenType type)
    {
        return rules[static_cast<int>(type)];
    }
    
    void binary(bool)
    {
        auto type = parser.prev.type;
        auto rule = getRule(type);
        parsePrec(static_cast<Prec>(static_cast<int>(rule.prec) + 1));

        switch (type)
        {
        case TokenType::BANG_EQUAL:
            emit(OpCode::EQUAL, OpCode::NOT); break;
        case TokenType::EQUAL_EQUAL:
            emit(OpCode::EQUAL); break;
        case TokenType::GREATER:
            emit(OpCode::GREATER); break;
        case TokenType::GREATER_EQUAL:
            emit(OpCode::LESS, OpCode::NOT); break;
        case TokenType::LESS:
            emit(OpCode::LESS); break;
        case TokenType::LESS_EQUAL:
            emit(OpCode::GREATER, OpCode::NOT); break;

        case TokenType::PLUS:
            emit(OpCode::ADD); break;
        case TokenType::MINUS:
            emit(OpCode::SUBTRACT); break;
        case TokenType::STAR:
            emit(OpCode::MULTIPLY); break;
        case TokenType::SLASH:
            emit(OpCode::DIVIDE); break;
        case TokenType::BANG:
            emit(OpCode::NOT); break;
        default:
            return;
        }
    }

    void grouping(bool)
    {
        expression();
        consume(TokenType::RIGHT_PAREN, "Expect ')' after expression.");
    }

    void unary(bool)
    {
        auto type = parser.prev.type;
        parsePrec(Prec::UNARY);

        switch (type)
        {
        case TokenType::BANG:
            emit(OpCode::NOT); break;
        case TokenType::MINUS:
            emit(OpCode::NEGATE); break;
        default:
            return;
        }
    }

    void parsePrec(Prec prec)
    {
        advance();
        auto pref = getRule(parser.prev.type).prefix;

        if(pref==nullptr)
        {
            error("Expect expression.");
            return;
        }

        bool canAssign = static_cast<int>(prec)<=static_cast<int>(Prec::ASSIGNMENT);

        (this->*pref)(canAssign);

        while(prec<=getRule(parser.cur.type).prec)
        {
            advance();
            auto inf = getRule(parser.prev.type).infix;
            (this->*inf)(canAssign);
        }

        if(canAssign && match(TokenType::EQUAL))
            error("Invalid assignment target.");
    }

    u8 parseVar(const string& msg)
    {
        consume(TokenType::IDENTIFIER, msg);

        declareVar();
        if(current->scopeDepth>0)
            return 0;

        return identConstant(parser.prev);
    }

    u8 identConstant(Token& name)
    {
        return makeConstant(Value(copyString(string_view(name.start, name.len))));
    }

    void literal(bool)
    {
        switch(parser.prev.type)
        {
        case TokenType::FALSE:
            emit(OpCode::FALSE); break;
        case TokenType::NIL:
            emit(OpCode::NIL); break;
        case TokenType::TRUE:
            emit(OpCode::TRUE); break;
        default:
            return;
        }
    }

    void stringy(bool) 
    {
        emitConstant(Value(copyString(std::string(
            parser.prev.start + 1,
            static_cast<size_t>(parser.prev.len - 2)))));
    }

    // ----------------

    void varDecl()
    {
        u8 global = parseVar("Expect variable name.");

        if(match(TokenType::EQUAL))
            expression();
        else
            emit(OpCode::NIL);
        
        consume(TokenType::SEMICOLON, "Expect ';' after variable decl.");
        defineVar(global);
    }

    void variable(bool canAssign)
    {
        namedVariable(parser.prev, canAssign);
    }

    void namedVariable(Token name, bool canAssign)
    {
        u8 get, set;
        int arg = resolveLocal(current, name);
        if(arg!=-1)
        {
            get = OpCode::GET_LOCAL;
            set = OpCode::SET_LOCAL;
        }
        else if((arg=resolveUpvalue(current, name))!=-1)
        {
            get = OpCode::GET_UPVALUE;
            set = OpCode::SET_UPVALUE;
        }
        else
        {
            arg = identConstant(name);
            get = OpCode::GET_GLOBAL;
            set = OpCode::SET_GLOBAL;
        }
        
        if(canAssign && match(TokenType::EQUAL))
        {
            expression();
            emit(set, static_cast<u8>(arg));
        }
        else
            emit(get, static_cast<u8>(arg));
    }

    void call(bool)
    {
        u8 argCount = argumentList();
        emit(OpCode::CALL, argCount);
    }

    void block()
    {
        while(!check(TokenType::RIGHT_BRACE) && !check(TokenType::TEOF))
            declaration();
        consume(TokenType::RIGHT_BRACE, "Expect '}' after block.");
    }

    void function_(FunctionType type)
    {
        Compiler compiler(type);
        beginScope();
        consume(TokenType::LEFT_PAREN, "Expect '(' after function name.");

        if(!check(TokenType::RIGHT_PAREN))
        {
            do
            {
                current->func->arity++;
                if(current->func->arity>255)
                    errorAtCur("Can't have more than 255 parameters.");
                u8 constant = parseVar("Expect parameter name.");
                defineVar(constant);
            } while (match(TokenType::COMMA));
        }

        consume(TokenType::RIGHT_PAREN, "Expect ')' after parameters.");
        consume(TokenType::LEFT_BRACE, "Expect '{' before function body.");
        block();

        ObjFunction* func = end();
        emit(OpCode::CLOSURE, makeConstant(Value(func)));

        for(int i=0; i<func->upvalCount; i++)
        {
            emit(compiler.upvalues[i].isLocal? 1: 0);
            emit(compiler.upvalues[i].index);
        }
    }

    void method()
    {
        consume(TokenType::IDENTIFIER, "Expect method name.");
        u8 constant = identConstant(parser.prev);

        FunctionType type = FunctionType::METHOD;
        if(string_view(parser.prev.start, static_cast<size_t>(parser.prev.len)) == "init")
            type = FunctionType::INIT;
        function_(type);

        emit(OpCode::METHOD, constant);
    }

    void classDecl()
    {
        consume(TokenType::IDENTIFIER, "Expect class name.");
        Token classname = parser.prev;
        u8 name = identConstant(parser.prev);
        declareVar();

        emit(OpCode::CLASS, name);
        defineVar(name);

        ClassCompiler classComp;
        classComp.hasSuper = false;
        classComp.enclosing = currentClass;
        currentClass = &classComp;

        if(match(TokenType::LESS))
        {
            consume(TokenType::IDENTIFIER, "Expect superclass name.");
            variable(false);
            if(identifiersEqual(classname, parser.prev))
                error("A class can't inherit from itself.");
            beginScope();
            addLocal(syntheticToken("super"));
            defineVar(0);
            namedVariable(classname, false);
            emit(OpCode::INHERIT);
            classComp.hasSuper = true;
        }

        namedVariable(classname, false);
        consume(TokenType::LEFT_BRACE, "Expect '{' before class body.");
        while(!check(TokenType::RIGHT_BRACE) && !check(TokenType::TEOF))
            method();
        consume(TokenType::RIGHT_BRACE, "Expect '}' after class body.");
        emit(OpCode::POP);

        if(classComp.hasSuper)
            endScope();
        currentClass = currentClass->enclosing;
    }

    void funDecl()
    {
        u8 global = parseVar("Expect function name.");
        markInitialized();
        function_(FunctionType::FUNCTION);
        defineVar(global);
    }

    void expressionStmt()
    {
        expression();
        consume(TokenType::SEMICOLON, "Expect ';' after expression.");
        emit(OpCode::POP);
    }

    void forStmt()
    {
        beginScope();
        consume(TokenType::LEFT_PAREN, "Expect '(' after 'for'.");

        if(match(TokenType::SEMICOLON))
        {
        }
        else if(match(TokenType::VAR))
        {
            varDecl();
        }
        else
        {
            expressionStmt();
        }

        int loopStart = currentChunk()->size();

        int exitJump = -1;
        if(!match(TokenType::SEMICOLON))
        {
            expression();
            consume(TokenType::SEMICOLON, "Expect ';' after loop condition.");
            exitJump = emitJump(OpCode::JUMP_IF_FALSE);
            emit(OpCode::POP);
        }

        if(!match(TokenType::RIGHT_PAREN))
        {
            int bodyJump = emitJump(OpCode::JUMP);
            int inc = currentChunk()->size();
            expression();
            emit(OpCode::POP);
            consume(TokenType::RIGHT_PAREN, "Expect ')' after for clauses.");

            emitLoop(loopStart);
            loopStart = inc;
            patchJump(bodyJump);
        }

        statement();
        emitLoop(loopStart);

        if(exitJump!=-1)
        {
            patchJump(exitJump);
            emit(OpCode::POP);
        }

        endScope();
    }

    void ifStmt()
    {
        consume(TokenType::LEFT_PAREN, "Expect '(' after 'if'.");
        expression();
        consume(TokenType::RIGHT_PAREN, "Expect ')' after condition.");

        int thenJump = emitJump(OpCode::JUMP_IF_FALSE);
        emit(OpCode::POP);
        statement();

        int elseJump = emitJump(OpCode::JUMP);

        patchJump(thenJump);
        emit(OpCode::POP);

        if(match(TokenType::ELSE))
            statement();
        patchJump(elseJump);
    }

    void printStmt()
    {
        expression();
        consume(TokenType::SEMICOLON, "Expect ';' after value.");
        emit(OpCode::PRINT);
    }

    void returnStmt()
    {
        if(current->ftype==FunctionType::SCRIPT)
            error("Can't return from top-level code.");

        if(match(TokenType::SEMICOLON))
            emitReturn();
        else
        {
            if(current->ftype==FunctionType::INIT)
                error("Can't return a value from an initializer.");

            expression();
            consume(TokenType::SEMICOLON, "Expect ';' after return value.");
            emit(OpCode::RETURN);
        }
    }

    void whileStmt()
    {
        int loopStart = currentChunk()->size();
        consume(TokenType::LEFT_PAREN, "Expect '(' after 'while'.");
        expression();
        consume(TokenType::RIGHT_PAREN, "Expect ')' after condition.");

        int exitJump = emitJump(OpCode::JUMP_IF_FALSE);
        emit(OpCode::POP);
        statement();
        emitLoop(loopStart);

        patchJump(exitJump);
        emit(OpCode::POP);
    }

    void beginScope()
    {
        current->scopeDepth++;
    }

    void endScope()
    {
        current->scopeDepth--;

        while(current->localCount>0 && current->locals[current->localCount-1].depth>current->scopeDepth)
        {
            if(current->locals[current->localCount-1].isCapt)
                emit(OpCode::CLOSE_UPVALUE);
            else
                emit(OpCode::POP);
            current->localCount--;
        }
    }

    bool identifiersEqual(Token& a, Token& b) const noexcept
    {
        if(a.len != b.len || a.len < 0)
            return false;
        if(a.len == 0)
            return true;
        if(a.start == nullptr || b.start == nullptr)
            return false;

        const auto length = static_cast<size_t>(a.len);
        return string_view(a.start, length) == string_view(b.start, length);
    }

    int resolveLocal(Compiler* compiler, Token& name)
    {
        for(int i=compiler->localCount-1; i>=0; i--)
        {
            Local* local = &compiler->locals[i];
            if(identifiersEqual(name, local->name))
            {
                if(local->depth==-1)
                    error("Can't read local variable in its own initializer.");
                return i;
            }
            
        }
        return -1;
    }

    int addUpvalue(Compiler* compiler, u8 index, bool isLocal)
    {
        int upvalCount = compiler->func->upvalCount;

        for(int i=0; i<upvalCount; i++)
        {
            auto upvalue = &compiler->upvalues[i];
            if(upvalue->index==index && upvalue->isLocal==isLocal)
                return i;
        }

        compiler->upvalues[upvalCount] = {index, isLocal};
        return compiler->func->upvalCount++;
    }

    int resolveUpvalue(Compiler* compiler, Token& name)
    {
        if(compiler->enclosing==nullptr)
            return -1;
        
        int local = resolveLocal(compiler->enclosing, name);
        if(local!=-1)
        {
            compiler->enclosing->locals[local].isCapt = true;
            return addUpvalue(compiler, static_cast<u8>(local), true);
        }
        
        int upvalue = resolveUpvalue(compiler->enclosing, name);
        if(upvalue!=-1)
        {
            return addUpvalue(compiler, static_cast<u8>(upvalue), false);
        }

        return -1;
    }

    void addLocal(Token name)
    {
        if(current->localCount==UINT8_COUNT)
        {
            error("Too many local variables in function.");
            return;
        }    

        Local* local = &current->locals[current->localCount++];
        local->name = name;
        local->depth = -1;
        local->isCapt = false;
    }

    void declareVar()
    {
        if(current->scopeDepth==0)
            return;
        Token& name = parser.prev;

        for (int i=current->localCount; i>=0; i--)
        {
            Local* local = &current->locals[i];
            if(local->depth!=-1 && local->depth < current->scopeDepth)
                break;
            if(identifiersEqual(name, local->name))
                error("Already a variable with this name in this scope.");
        }
        
        addLocal(name);
    }

    void markInitialized()
    {
        if(current->scopeDepth==0)
            return;
        current->locals[current->localCount-1].depth = current->scopeDepth;
    }

    void emitLoop(int loopStart)
    {
        emit(OpCode::LOOP);
        int offset = currentChunk()->size()-loopStart+2;
        if(offset>UINT16_MAX)
            error("Loop body too large.");
        emit((offset >> 8) & 0xff);
        emit(offset & 0xff);
    }

    int emitJump(u8 instruction)
    {
        emit(instruction);
        emit(0xff);
        emit(0xff);
        return currentChunk()->size()-2;
    }
    void patchJump(int offset)
    {
        int jump = currentChunk()->size()-offset-2;

        if(jump>UINT16_MAX)
            error("Too much code to jump over.");
        currentChunk()->code[offset] = (jump>>8) & 0xff;
        currentChunk()->code[offset+1] = jump & 0xff;
    }

    void defineVar(u8 global)
    {
        if(current->scopeDepth>0)
        {
            markInitialized();
            return;
        }
        
        emit(OpCode::DEFINE_GLOBAL, global);
    }

    void and_(bool)
    {
        int endJump = emitJump(OpCode::JUMP_IF_FALSE);
        emit(OpCode::POP);
        parsePrec(Prec::AND);
        patchJump(endJump);
    }

    void or_(bool)
    {
        int elseJump = emitJump(OpCode::JUMP_IF_FALSE);
        int endJump = emitJump(OpCode::JUMP);
        patchJump(elseJump);
        emit(OpCode::POP);
        parsePrec(Prec::OR);
        patchJump(endJump);
    }

    void dot(bool canAssign)
    {
        consume(TokenType::IDENTIFIER, "Expect property name after '.'.");
        u8 name = identConstant(parser.prev);

        if(canAssign && match(TokenType::EQUAL))
        {
            expression();
            emit(OpCode::SET_PROPERTY, name);
        }
        else if(match(TokenType::LEFT_PAREN))
        {
            u8 argCount = argumentList();
            emit(OpCode::INVOKE, name);
            emit(argCount);
        }
        else
        {
            emit(OpCode::GET_PROPERTY, name);
        }
    }

    void this_(bool)
    {
        if(currentClass==nullptr)
        {
            error("Can't use 'this' outside of a class.");
            return;
        }

        variable(false);
    }

    void super_(bool)
    {
        if(currentClass == nullptr)
            error("Can't use 'super' outside of a class.");
        else if(!currentClass->hasSuper)
            error("Can't use 'super' in a class with no superclass.");

        consume(TokenType::DOT, "Expect '.' after 'super'.");
        consume(TokenType::IDENTIFIER, "Expect superclass method name.");
        u8 name = identConstant(parser.prev);

        namedVariable(syntheticToken("this"), false);
        if(match(TokenType::LEFT_PAREN))
        {
            u8 argCount = argumentList();
            namedVariable(syntheticToken("super"), false);
            emit(OpCode::SUPER_INVOKE, name);
            emit(argCount);
        }
        else
        {
            namedVariable(syntheticToken("super"), false);
            emit(OpCode::GET_SUPER, name);
        }
    }

    u8 argumentList()
    {
        u8 argCount = 0;
        if(!check(TokenType::RIGHT_PAREN))
        {
            do
            {
                expression();
                if(argCount==255)
                    error("Can't have more than 255 arguments.");
                argCount++;
            } while (match(TokenType::COMMA));
            
        }

        consume(TokenType::RIGHT_PAREN, "Expect ')' after arguments.");
        return argCount;
    }

    Token syntheticToken(const string& text)
    {
        Token token;
        token.start = text.c_str();
        token.len = static_cast<int>(text.size());
        return token;
    }

    void synchronize()
    {
        parser.panic = false;
        while(parser.cur.type != TokenType::TEOF)
        {
            if(parser.prev.type == TokenType::SEMICOLON)
                return;
            switch (parser.cur.type)
            {
            case TokenType::CLASS:
            case TokenType::FUN:
            case TokenType::VAR:
            case TokenType::FOR:
            case TokenType::IF:
            case TokenType::WHILE:
            case TokenType::PRINT:
            case TokenType::RETURN:
                return;
            default:
                ;
            }
            advance();
        }
    }

    Chunk* currentChunk()
    {
        return &current->func->chunk;
    }

};

Rules RulesMaker::make() noexcept
{
    pair<TokenType, ParseRule> temp[] = {
        {TokenType::LEFT_PAREN, ParseRule(&Compiler::grouping, &Compiler::call, Prec::CALL)},
        {TokenType::RIGHT_PAREN, ParseRule()},
        {TokenType::LEFT_BRACE, ParseRule()},
        {TokenType::COMMA, ParseRule()},
        {TokenType::DOT, ParseRule(nullptr, &Compiler::dot, Prec::CALL)},
        {TokenType::MINUS, ParseRule(&Compiler::unary, &Compiler::binary, Prec::TERM)},
        {TokenType::PLUS, ParseRule(nullptr, &Compiler::binary, Prec::TERM)},
        {TokenType::SEMICOLON, ParseRule()},
        {TokenType::SLASH, ParseRule(nullptr, &Compiler::binary, Prec::FACTOR)},
        {TokenType::STAR, ParseRule(nullptr, &Compiler::binary, Prec::FACTOR)},
        {TokenType::BANG, ParseRule(&Compiler::unary, nullptr, Prec::NONE)},
        {TokenType::BANG_EQUAL, ParseRule(nullptr, &Compiler::binary, Prec::EQUALITY)},
        {TokenType::EQUAL, ParseRule()},
        {TokenType::EQUAL_EQUAL, ParseRule(nullptr, &Compiler::binary, Prec::EQUALITY)},
        {TokenType::GREATER, ParseRule(nullptr, &Compiler::binary, Prec::COMPARISON)},
        {TokenType::GREATER_EQUAL, ParseRule(nullptr, &Compiler::binary, Prec::COMPARISON)},
        {TokenType::LESS, ParseRule(nullptr, &Compiler::binary, Prec::COMPARISON)},
        {TokenType::LESS_EQUAL, ParseRule(nullptr, &Compiler::binary, Prec::COMPARISON)},
        {TokenType::IDENTIFIER, ParseRule(&Compiler::variable, nullptr, Prec::NONE)},
        {TokenType::STRING, ParseRule(&Compiler::stringy, nullptr, Prec::NONE)},
        {TokenType::NUMBER, ParseRule(&Compiler::number, nullptr, Prec::NONE)},
        {TokenType::AND, ParseRule(nullptr, &Compiler::and_, Prec::AND)},
        {TokenType::CLASS, ParseRule()},
        {TokenType::ELSE, ParseRule()},
        {TokenType::FALSE, ParseRule(&Compiler::literal, nullptr, Prec::NONE)},
        {TokenType::FOR, ParseRule()},
        {TokenType::FUN, ParseRule()},
        {TokenType::IF, ParseRule()},
        {TokenType::NIL, ParseRule(&Compiler::literal, nullptr, Prec::NONE)},
        {TokenType::OR, ParseRule(nullptr, &Compiler::or_, Prec::OR)},
        {TokenType::PRINT, ParseRule()},
        {TokenType::RETURN, ParseRule()},
        {TokenType::SUPER, ParseRule(&Compiler::super_, nullptr, Prec::NONE)},
        {TokenType::THIS, ParseRule(&Compiler::this_, nullptr, Prec::NONE)},
        {TokenType::TRUE, ParseRule(&Compiler::literal, nullptr, Prec::NONE)},
        {TokenType::VAR, ParseRule()},
        {TokenType::WHILE, ParseRule()},
        {TokenType::ERROR, ParseRule()},
        {TokenType::TEOF, ParseRule()},
    };

    Rules rules{};

    for(auto it = std::begin(temp); it != std::end(temp); ++it)
        rules[static_cast<size_t>(it->first)] = it->second;
    return rules;
}

Compiler Compiler::comp;
