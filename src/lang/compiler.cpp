#include "compiler.h"
#include "vm.h"

Compiler::Compiler(VM& vm, FunctionType type): enclosing(current), owner(&vm), ftype(type)
{
    current = this;

    ObjString* functionName = nullptr;
    bool nameRooted = false;

    try
    {
        if(type == FunctionType::LAMBDA)
        {
            functionName = copyString(vm, "(lambda)");
        }
        else if(type != FunctionType::SCRIPT && enclosing != nullptr)
        {
            functionName = copyString(vm, std::string_view(
                enclosing->parser.prev.start,
                static_cast<size_t>(enclosing->parser.prev.len)));
            vm.push(Value(functionName));
            nameRooted = true;
        }

        func = makeObj<ObjFunction>(vm, functionName);
    }
    catch(...)
    {
        if(nameRooted)
            vm.pop();
        current = enclosing;
        throw;
    }

    if(nameRooted)
        vm.pop();

    Local* local = &locals[localCount++];
    local->depth = 0;
    local->isCapt = false;

    if(type == FunctionType::METHOD || type == FunctionType::INIT)
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

ObjFunction* Compiler::compile(VM& vm, const string& src, bool repl)
{
    this->repl = repl;

    enclosing = nullptr;
    owner = &vm;
    ftype = FunctionType::SCRIPT;
    func = nullptr;
    localCount = 0;
    scopeDepth = 0;
    currentClass = nullptr;
    currentLoop = nullptr;
    current = this;
    func = makeObj<ObjFunction>(vm);

    Local* local = &locals[localCount++];
    local->depth = 0;
    local->isCapt = false;
    local->name.start = "";
    local->name.len = 0;

    parser = Parser{};
    this->chunk = &func->chunk;
    scanner = std::make_unique<Scanner>(src, repl);
    
    advance();
    while(!isAtEnd())
        declaration();

    auto res = end();
    return parser.hadError? nullptr: res;
}

void Compiler::markCompilerRoots(VM& vm)
{
    Compiler* compiler = current;
    while(compiler!=nullptr)
    {
        vm.markObject(static_cast<Obj*>(compiler->func));
        compiler = compiler->enclosing;
    }
}

void Compiler::errorAt(Token& token, const string& msg)
{
    if(parser.panic)
        return;
    parser.panic = true;

    std::cerr<<"[compile error] line "<<token.line;

    if(token.type == TokenType::TEOF || token.type == TokenType::REPL_EOF)
        std::cerr<<" at end";
    else if(token.type != TokenType::ERROR && token.start != nullptr && token.len > 0)
    {
        std::cerr<<" at '";
        std::cerr.write(token.start, token.len);
        std::cerr<<"'";
    }

    std::cerr<<": "<<msg<<'\n';
    parser.hadError = true;
}

void Compiler::error(const string& msg)
{
    errorAt(parser.prev, msg);
}

void Compiler::errorAtCur(const string& msg)
{
    errorAt(parser.cur, msg);
}

ObjFunction* Compiler::end()
{
    emitReturn();
    auto func = current->func;
    
    #ifdef DEBUG_PRINT_CODE
    if(!parser.hadError)
        Debug::disassembleChunk(*currentChunk(),
            func->name != nullptr? func->name->str() : "<script>");
    #endif

    current = current->enclosing;
    return func;
}

void Compiler::number(bool)
{
    double val = strtod(parser.prev.start, nullptr);
    emitConstant(Value(val));
}

void Compiler::imaginary(bool)
{
    const double imaginary = strtod(parser.prev.start, nullptr);
    auto* value = makeObj<ObjComplex>(*owner, 0.0, imaginary);
    emitConstant(Value(value));
}

void Compiler::emitReturn()
{
    if(current->ftype==FunctionType::INIT)
        emit(OpCode::GET_LOCAL, 0);
    else
        emit(OpCode::NIL);
    emit(OpCode::RETURN);
}

u8 Compiler::makeConstant(Value value)
{
    owner->push(value);

    int constant;
    try
    {
        constant = currentChunk()->addConst(value);
    }
    catch(...)
    {
        owner->pop();
        throw;
    }
    owner->pop();

    if(constant < 0 || constant > UINT8_MAX)
    {
        error("Too many constants in one chunk.");
        return 0;
    }
    return static_cast<u8>(constant);
}

void Compiler::emitConstant(Value value)
{
    emit(OpCode::CONSTANT, makeConstant(value));
}

void Compiler::expression()
{
    parsePrec(Prec::ASSIGNMENT);
}

void Compiler::statement()
{
    if(match(TokenType::PRINT))
    {
        printStmt();
    }
    else if(match(TokenType::ASSERT))
    {
        assertStmt();
    }
    else if(match(TokenType::EXTEND))
    {
        extendStmt();
    }
    else if(match(TokenType::FOR))
    {
        forStmt();
    }
    else if(match(TokenType::IF))
    {
        ifStmt();
    }
    else if(match(TokenType::BREAK))
    {
        breakStmt();
    }
    else if(match(TokenType::CONTINUE))
    {
        contStmt();
    }
    else if(match(TokenType::FAIL))
    {
        failStmt();
    }
    else if(match(TokenType::RETURN))
    {
        returnStmt();
    }
    else if(match(TokenType::WHILE))
    {
        whileStmt();
    }
    else if(match(TokenType::RANGE))
    {
        rangeStmt();
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

void Compiler::declaration()
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

const ParseRule& Compiler::getRule(TokenType type) const noexcept
{
    return rules[static_cast<size_t>(type)];
}

void Compiler::binary(bool)
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
    case TokenType::APPROX_EQUAL:
        emit(OpCode::APPROX); break;

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

void Compiler::grouping(bool)
{
    expression();
    consume(TokenType::RIGHT_PAREN, "Expect ')' after expression.");
}

void Compiler::unary(bool)
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

void Compiler::parsePrec(Prec prec)
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

u8 Compiler::parseVar(const string& msg)
{
    consume(TokenType::IDENTIFIER, msg);

    declareVar();
    if(current->scopeDepth>0)
        return 0;

    return identConstant(parser.prev);
}

u8 Compiler::identConstant(Token& name)
{
    return makeConstant(Value(copyString(*owner,
        std::string_view(name.start, name.len))));
}

void Compiler::literal(bool)
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

void Compiler::stringy(bool) 
{
    emitConstant(Value(copyString(*owner, std::string_view(
        parser.prev.start + 1,
        static_cast<size_t>(parser.prev.len - 2)))));
}

void Compiler::varDecl()
{
    u8 global = parseVar("Expect variable name.");

    if(match(TokenType::EQUAL))
        expression();
    else
        emit(OpCode::NIL);
    
    consumeEnd("Expect ';' after variable decl.");
    defineVar(global);
}

void Compiler::variable(bool canAssign)
{
    namedVariable(parser.prev, canAssign);
}

void Compiler::namedVariable(Token name, bool canAssign)
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

void Compiler::call(bool)
{
    u8 argCount = argumentList();
    emit(OpCode::CALL, argCount);
}

void Compiler::block()
{
    while(!check(TokenType::RIGHT_BRACE) && !check(TokenType::TEOF))
        declaration();
    consume(TokenType::RIGHT_BRACE, "Expect '}' after block.");
}

void Compiler::function_(FunctionType type)
{
    Compiler compiler(*owner, type);
    beginScope();
    consume(TokenType::LEFT_PAREN, "Expect '(' after function name.");

    parameterList();

    consume(TokenType::LEFT_BRACE, "Expect '{' before function body.");
    block();

    ObjFunction* func = end();
    emitClosure(compiler, func);
}

void Compiler::method(bool isStatic)
{
    consume(TokenType::IDENTIFIER, "Expect method name.");
    declareClassMember(parser.prev);
    u8 constant = identConstant(parser.prev);

    FunctionType type = isStatic ? FunctionType::STATIC_METHOD : FunctionType::METHOD;
    if(!isStatic &&
        std::string_view(parser.prev.start, static_cast<size_t>(parser.prev.len)) == "init")
        type = FunctionType::INIT;
    function_(type);

    emit(isStatic ? OpCode::STATIC_METHOD : OpCode::METHOD, constant);
}

void Compiler::classDecl()
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
        superclass(classname);
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
        classMember();
    consume(TokenType::RIGHT_BRACE, "Expect '}' after class body.");
    emit(OpCode::POP);

    if(classComp.hasSuper)
        endScope();
    currentClass = currentClass->enclosing;
}

void Compiler::funDecl()
{
    u8 global = parseVar("Expect function name.");
    markInitialized();
    function_(FunctionType::FUNCTION);
    defineVar(global);
}

void Compiler::expressionStmt()
{
    expression();

    bool flag = 
        current->ftype == FunctionType::INIT &&
        current->scopeDepth == 1 &&
        check(TokenType::RIGHT_BRACE);

    if(flag)
    {
        error("Cannot make implicit returns from initializer.");
        emit(OpCode::POP_UNHANDLED);
        return;
    }

    flag = 
        current->ftype != FunctionType::SCRIPT &&
        current->ftype != FunctionType::INIT &&
        current->scopeDepth == 1 &&
        check(TokenType::RIGHT_BRACE);

    if(flag)
    {
        emit(OpCode::RETURN);
        return;
    }

    bool semicolon = consumeEnd("Expect ';' after expression.");

    if(current->repl && !semicolon)
        emit(OpCode::REPL_RESULT);
    else
        emit(OpCode::POP_UNHANDLED);
}

void Compiler::failStmt()
{
    if(current->ftype == FunctionType::SCRIPT)
        error("Can't fail from top-level code.");

    consume(TokenType::IDENTIFIER, "Expect error kind after 'fail'.");
    emit(OpCode::CONSTANT, errorKindConstant(parser.prev));

    consume(TokenType::COMMA, "Expect ',' after error kind.");
    expression();

    if(match(TokenType::COMMA))
        expression();
    else
        emit(OpCode::NIL);

    consume(TokenType::SEMICOLON, "Expect ';' after fail statement.");
    emit(OpCode::FAIL);
}

void Compiler::forStmt()
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

    LoopCompiler loop(owner);
    loop.enclosing = current->currentLoop;
    loop.scopeDepth = current->scopeDepth;
    loop.continueTarget = loopStart;
    current->currentLoop = &loop;

    statement();
    emitLoop(loopStart);

    if(exitJump!=-1)
    {
        patchJump(exitJump);
        emit(OpCode::POP);
    }

    patchBreaks(loop);
    current->currentLoop = loop.enclosing;
    endScope();
}

void Compiler::ifStmt()
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

void Compiler::printStmt()
{
    expression();
    consumeEnd("Expect ';' after printed value.");
    emit(OpCode::PRINT);
}

void Compiler::assertStmt()
{
    expression();
    consumeEnd("Expect ';' after assertion value");
    emit(OpCode::ASSERT);
}

void Compiler::returnStmt()
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

void Compiler::whileStmt()
{
    int loopStart = currentChunk()->size();
    consume(TokenType::LEFT_PAREN, "Expect '(' after 'while'.");
    expression();
    consume(TokenType::RIGHT_PAREN, "Expect ')' after condition.");

    int exitJump = emitJump(OpCode::JUMP_IF_FALSE);
    emit(OpCode::POP);

    LoopCompiler loop(owner);
    loop.enclosing = current->currentLoop;
    loop.scopeDepth = current->scopeDepth;
    loop.continueTarget = loopStart;
    current->currentLoop = &loop;

    statement();
    emitLoop(loopStart);

    patchJump(exitJump);
    emit(OpCode::POP);
    patchBreaks(loop);
    current->currentLoop = loop.enclosing;
}

void Compiler::breakStmt()
{
    LoopCompiler* loop = current->currentLoop;
    if(loop==nullptr)
        error("Can't use 'break' outside of a loop.");

    consume(TokenType::SEMICOLON, "Expect ';' after break.");

    if(loop==nullptr)
        return;

    emitLoopCleanup(*loop);
    loop->breakJumps.push_back(emitJump(OpCode::JUMP));
}

void Compiler::contStmt()
{
    LoopCompiler* loop = current->currentLoop;
    if(loop==nullptr)
        error("Can't use 'continue' outside of a loop.");

    consume(TokenType::SEMICOLON, "Expect ';' after continue.");

    if(loop==nullptr)
        return;

    emitLoopCleanup(*loop);
    emitLoop(loop->continueTarget);
}

void Compiler::emitLoopCleanup(const LoopCompiler& loop)
{
    for(int i=current->localCount-1; i>=0; i--)
    {
        const Local& local = current->locals[i];
        if(local.depth<=loop.scopeDepth)
            break;

        emit(local.isCapt? OpCode::CLOSE_UPVALUE: OpCode::POP);
    }
}

void Compiler::patchBreaks(const LoopCompiler& loop)
{
    for(int jump: loop.breakJumps)
        patchJump(jump);
}

void Compiler::beginScope()
{
    current->scopeDepth++;
}

void Compiler::endScope()
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

bool Compiler::identifiersEqual(Token& a, Token& b) const
{
    if(a.len != b.len || a.len < 0)
        return false;
    if(a.len == 0)
        return true;
    if(a.start == nullptr || b.start == nullptr)
        return false;

    const auto length = static_cast<size_t>(a.len);
    return std::string_view(a.start, length) == std::string_view(b.start, length);
}

int Compiler::resolveLocal(Compiler* compiler, Token& name)
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

int Compiler::addUpvalue(Compiler* compiler, u8 index, bool isLocal)
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

int Compiler::resolveUpvalue(Compiler* compiler, Token& name)
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

void Compiler::addLocal(Token name)
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

void Compiler::declareVar()
{
    if(current->scopeDepth==0)
        return;
    Token& name = parser.prev;

    for (int i=current->localCount-1; i>=0; i--)
    {
        Local* local = &current->locals[i];
        if(local->depth!=-1 && local->depth < current->scopeDepth)
            break;
        if(identifiersEqual(name, local->name))
            error("Already a variable with this name in this scope.");
    }
    
    addLocal(name);
}

void Compiler::markInitialized()
{
    if(current->scopeDepth==0)
        return;
    current->locals[current->localCount-1].depth = current->scopeDepth;
}

void Compiler::emitLoop(int loopStart)
{
    emit(OpCode::LOOP);
    int offset = currentChunk()->size()-loopStart+2;
    if(offset>UINT16_MAX)
        error("Loop body too large.");
    emit((offset >> 8) & 0xff);
    emit(offset & 0xff);
}

int Compiler::emitJump(u8 instruction)
{
    emit(instruction);
    emit(0xff);
    emit(0xff);
    return currentChunk()->size()-2;
}

void Compiler::patchJump(int offset)
{
    int jump = currentChunk()->size()-offset-2;

    if(jump>UINT16_MAX)
        error("Too much code to jump over.");
    currentChunk()->code[offset] = (jump>>8) & 0xff;
    currentChunk()->code[offset+1] = jump & 0xff;
}

void Compiler::defineVar(u8 global)
{
    if(current->scopeDepth>0)
    {
        markInitialized();
        return;
    }
    
    emit(OpCode::DEFINE_GLOBAL, global);
}

void Compiler::and_(bool)
{
    int endJump = emitJump(OpCode::JUMP_IF_FALSE);
    emit(OpCode::POP);
    parsePrec(Prec::AND);
    patchJump(endJump);
}

void Compiler::or_(bool)
{
    int elseJump = emitJump(OpCode::JUMP_IF_FALSE);
    int endJump = emitJump(OpCode::JUMP);
    patchJump(elseJump);
    emit(OpCode::POP);
    parsePrec(Prec::OR);
    patchJump(endJump);
}

void Compiler::else_(bool)
{
    emit(OpCode::IS_ERROR);
    int keepLeftJump = emitJump(OpCode::JUMP_IF_FALSE);
    emit(OpCode::POP);
    emit(OpCode::POP);
    parsePrec(Prec::ASSIGNMENT);
    int doneJump = emitJump(OpCode::JUMP);
    patchJump(keepLeftJump);
    emit(OpCode::POP);
    patchJump(doneJump);
}

void Compiler::dot(bool canAssign)
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

void Compiler::this_(bool)
{
    if(currentClass==nullptr)
    {
        error("Can't use 'this' outside of a class.");
        return;
    }

    if(methodContext() == MethodContext::STATIC)
    {
        error("Can't use 'this' in a static method.");
        return;
    }

    variable(false);
}

void Compiler::super_(bool)
{
    if(currentClass == nullptr)
    {
        error("Can't use 'super' outside of a class.");
        return;
    }
    else if(!currentClass->hasSuper)
    {
        error("Can't use 'super' in a class with no superclass.");
        return;
    }
    else if(methodContext() == MethodContext::STATIC)
    {
        error("Can't use 'super' in a static method.");
        return;
    }

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

MethodContext Compiler::methodContext() const
{
    for(Compiler* compiler = current; compiler != nullptr; compiler = compiler->enclosing)
    {
        switch(compiler->ftype)
        {
        case FunctionType::METHOD:
        case FunctionType::INIT:
            return MethodContext::INSTANCE;
        case FunctionType::STATIC_METHOD:
            return MethodContext::STATIC;
        case FunctionType::FUNCTION:
        case FunctionType::SCRIPT:
        case FunctionType::LAMBDA:

            break;
        }
    }
    return MethodContext::NONE;
}

u8 Compiler::errorKindConstant(Token kind)
{
    const std::string_view name(kind.start, static_cast<size_t>(kind.len));

    ErrorKind errorKind;
    if(name == "DOMAIN" || name == "DOMAIN_ERROR")
        errorKind = ErrorKind::DOMAIN_ERROR;
    else if(name == "RANGE" || name == "RANGE_ERROR")
        errorKind = ErrorKind::RANGE_ERROR;
    else if(name == "TYPE" || name == "TYPE_ERROR")
        errorKind = ErrorKind::TYPE_ERROR;
    else if(name == "INDEX" || name == "INDEX_ERROR")
        errorKind = ErrorKind::INDEX_ERROR;
    else if(name == "IO" || name == "IO_ERROR")
        errorKind = ErrorKind::IO_ERROR;
    else if(name == "VALUE" || name == "VALUE_ERROR")
        errorKind = ErrorKind::VALUE_ERROR;
    else if(name == "NAME" || name == "NAME_ERROR")
        errorKind = ErrorKind::NAME_ERROR;
    else if(name == "USER" || name == "USER_ERROR")
        errorKind = ErrorKind::USER_ERROR;
    else
    {
        error("Unknown error kind.");
        return 0;
    }

    return makeConstant(Value(static_cast<double>(static_cast<u8>(errorKind))));
}

u8 Compiler::argumentList()
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

Token Compiler::syntheticToken(const char* text)
{
    Token token;
    token.start = text;
    token.len = static_cast<int>(strlen(text));
    return token;
}

void Compiler::synchronize()
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
        case TokenType::RANGE:
        case TokenType::IF:
        case TokenType::WHILE:
        case TokenType::PRINT:
        case TokenType::RETURN:
        case TokenType::FAIL:
        case TokenType::BREAK:
        case TokenType::CONTINUE:
            return;
        default:
            ;
        }
        advance();
    }
}

// -----

void Compiler::arrayLit(bool)
{
    int count = 0;
    if(!check(TokenType::RIGHT_SQUARE))
    {
        do
        {
            if(count==UINT8_MAX)
                error("Can't have more than 255 elems in array literal.");
            expression();
            count++;
        } while (match(TokenType::COMMA) && !check(TokenType::RIGHT_SQUARE));
        
    }

    consume(TokenType::RIGHT_SQUARE, "Expect ']' after array elems in literal.");
    emit(OpCode::MAKE_ARRAY, static_cast<u8>(count));
}

void Compiler::subscript(bool canAssign)
{
    expression();
    consume(TokenType::RIGHT_SQUARE, "Expect ']' after array index.");

    if (canAssign && match(TokenType::EQUAL))
    {
        expression();
        emit(OpCode::SET_INDEX);
    }
    else
    {
        emit(OpCode::GET_INDEX);
    }
}

void Compiler::lambda(bool)
{
    Compiler compiler(*owner, FunctionType::LAMBDA);
    beginScope();

    if(match(TokenType::LEFT_PAREN))
        parameterList();
    else
        parameter();

    consume(TokenType::RIGHT_ARROW,"Expect '=>' after lambda parameters.");

    expression();
    emit(OpCode::RETURN);
    ObjFunction* function = end();
    emitClosure(compiler, function);
}

void Compiler::parameter()
{
    current->func->arity++;
    if(current->func->arity > 255)
        errorAtCur("Can't have more than 255 parameters.");
    u8 parameter = parseVar("Expect parameter name.");
    defineVar(parameter);
}

void Compiler::parameterList()
{
    if(check(TokenType::RIGHT_PAREN))
    {
        advance();
        return;
    }

    while(true)
    {
        u8 parameter = parseVar("Expect parameter name.");
        defineVar(parameter);
        if(match(TokenType::ELLIPSIS))
        {
            current->func->variadic = true;
            if(match(TokenType::COMMA))
                error("Rest parameter must be the final parameter.");
            break;
        }

        current->func->arity++;
        if(current->func->arity > 255)
            error("Can't have more than 255 parameters.");

        if(!match(TokenType::COMMA))
            break;
    }

    consume(TokenType::RIGHT_PAREN, "Expect ')' after parameters.");
}

void Compiler::emitClosure(const Compiler& compiler, ObjFunction* func)
{
    emit(OpCode::CLOSURE, makeConstant(Value(func)));
    for(int i=0; i<func->upvalCount; i++)
    {
        emit(compiler.upvalues[i].isLocal? 1: 0);
        emit(compiler.upvalues[i].index);
    }
}

void Compiler::extendStmt()
{
    if(current->ftype != FunctionType::SCRIPT)
        error("Extension methods can only be declared at top level.");

    consume(TokenType::STRING, "Expect native type name.");
    emitConstant(Value(copyString(*owner, std::string_view(
            parser.prev.start + 1,
            static_cast<size_t>(parser.prev.len - 2)))));

    consume(TokenType::STRING, "Expect extension method name.");
    emitConstant(Value(copyString(*owner, std::string_view(
        parser.prev.start + 1,
        static_cast<size_t>(parser.prev.len - 2)))));

    expression();
    consumeEnd("Expect ';' after extension declaration.");
    emit(OpCode::EXTEND);
}

void Compiler::rangeStmt()
{
    beginScope();
    consume(TokenType::LEFT_PAREN, "Expect '(' after 'range'.");
    consume(TokenType::IDENTIFIER, "Expect variable for 'range'.");
    Token first = parser.prev;
    std::optional<Token> second;

    if(match(TokenType::COMMA))
    {
        consume(TokenType::IDENTIFIER, "Expect second range variable after ','.");
        second = parser.prev;
        if(identifiersEqual(first, *second))
            errorAt(parser.prev, "Range variables must be different.");
    }

    consume(TokenType::COLON, "Expect ':' after range variables.");
    expression();
    consume(TokenType::RIGHT_PAREN, "Expect ')' after range expression.");

    const u8 width = second.has_value() ? 2 : 1;
    emit(OpCode::ITER_SNAP, width);

    // The snapshot produced by ITER_SNAP is already on the stack. Binding it
    // as a local keeps it rooted for the lifetime of the loop.
    addLocal(syntheticToken("@range_snapshot"));
    markInitialized();
    const int snapshotSlot = current->localCount - 1;

    emitConstant(Value(0.0));
    addLocal(syntheticToken("@range_cursor"));
    markInitialized();
    const int cursorSlot = current->localCount - 1;

    emit(OpCode::NIL);
    addLocal(first);
    markInitialized();
    const int firstSlot = current->localCount - 1;

    int secondSlot = -1;
    if(second.has_value())
    {
        emit(OpCode::NIL);
        addLocal(*second);
        markInitialized();
        secondSlot = current->localCount - 1;
    }

    const int conditionStart = currentChunk()->size();

    emit(OpCode::GET_LOCAL, static_cast<u8>(cursorSlot));
    emit(OpCode::GET_LOCAL, static_cast<u8>(snapshotSlot));
    Token length = syntheticToken("len");
    emit(OpCode::GET_PROPERTY, identConstant(length));
    emit(OpCode::LESS);

    const int exitJump = emitJump(OpCode::JUMP_IF_FALSE);
    emit(OpCode::POP);

    // first = snapshot[cursor]
    emit(OpCode::GET_LOCAL, static_cast<u8>(snapshotSlot));
    emit(OpCode::GET_LOCAL, static_cast<u8>(cursorSlot));
    emit(OpCode::GET_INDEX);
    emit(OpCode::SET_LOCAL, static_cast<u8>(firstSlot));
    emit(OpCode::POP);

    if(second.has_value())
    {
        // second = snapshot[cursor + 1]
        emit(OpCode::GET_LOCAL, static_cast<u8>(snapshotSlot));
        emit(OpCode::GET_LOCAL, static_cast<u8>(cursorSlot));
        emitConstant(Value(1.0));
        emit(OpCode::ADD);
        emit(OpCode::GET_INDEX);
        emit(OpCode::SET_LOCAL, static_cast<u8>(secondSlot));
        emit(OpCode::POP);
    }

    // Advance before the body so continue can jump to the condition directly.
    emit(OpCode::GET_LOCAL, static_cast<u8>(cursorSlot));
    emitConstant(Value(static_cast<double>(width)));
    emit(OpCode::ADD);
    emit(OpCode::SET_LOCAL, static_cast<u8>(cursorSlot));
    emit(OpCode::POP);

    LoopCompiler loop(owner);
    loop.enclosing = current->currentLoop;
    loop.scopeDepth = current->scopeDepth;
    loop.continueTarget = conditionStart;
    current->currentLoop = &loop;

    statement();
    emitLoop(conditionStart);

    patchJump(exitJump);
    emit(OpCode::POP);
    patchBreaks(loop);
    current->currentLoop = loop.enclosing;
    endScope();
}

void Compiler::classMember()
{
    if(match(TokenType::CLASS))
    {
        nestedClassDecl();
        return;
    }

    if(match(TokenType::STATIC))
    {
        if(match(TokenType::CLASS))
            nestedClassDecl();
        else
            method(true);
        return;
    }

    method(false);
}

void Compiler::nestedClassDecl()
{
    consume(TokenType::IDENTIFIER, "Expect nested class name.");
    Token classname = parser.prev;
    declareClassMember(classname);
    u8 name = identConstant(classname);

    addLocal(syntheticToken("@nested_owner"));
    markInitialized();

    bool hasSuper = false;

    if(match(TokenType::LESS))
    {
        superclass(classname);
        hasSuper = true;

        beginScope();
        addLocal(syntheticToken("super"));
        defineVar(0);
    }

    emit(OpCode::NESTED_CLASS, name);
    emit(static_cast<u8>(hasSuper));

    ClassCompiler nested;
    nested.hasSuper = hasSuper;
    nested.enclosing = currentClass;
    currentClass = &nested;

    consume(TokenType::LEFT_BRACE, "Expect '{' before nested class body.");
    while(!check(TokenType::RIGHT_BRACE) && !check(TokenType::TEOF))
    {
        classMember();
    }
    consume(TokenType::RIGHT_BRACE, "Expect '}' after nested class body.");
    emit(OpCode::POP);
    if(hasSuper)
        endScope();

    // NESTED_CLASS leaves the enclosing class on the stack for the rest of its
    // body. Remove only the compiler bookkeeping entry; do not emit a pop.
    --localCount;
    currentClass = nested.enclosing;
}

void Compiler::superclass(Token subclassName)
{
    consume(TokenType::IDENTIFIER, "Expect superclass name.");
    Token root = parser.prev;
    namedVariable(root, false);
    bool qualified = false;

    while(match(TokenType::DOT))
    {
        qualified = true;
        consume(TokenType::IDENTIFIER, "Expect name after '.' in superclass path.");
        emit(OpCode::GET_PROPERTY, identConstant(parser.prev));
    }

    if(!qualified && identifiersEqual(subclassName, root))
        error("Class cannot inherit from itself.");
}

void Compiler::declareClassMember(Token& name)
{
    if(currentClass == nullptr)
        return;

    const std::string_view text(
        name.start,
        static_cast<size_t>(name.len));

    if(findNativeProperty(ObjType::CLASS, text) != nullptr ||
        findNativeMethod(ObjType::CLASS, text) != nullptr)
    {
        errorAt(name,
            "Class member name conflicts with a built-in class attribute.");
    }

    if(!currentClass->declaredMembers.insert(text).second)
        errorAt(name, "Class already contains a member with this name.");
}

// -----

consteval Rules RulesMaker::make() noexcept
{
    Rules result{};

    auto set = [&result](TokenType type, ParseFn prefix, ParseFn infix, Prec prec) constexpr noexcept {
        result[static_cast<size_t>(type)] = ParseRule{prefix, infix, prec};
    };

    set(TokenType::LEFT_PAREN, &Compiler::grouping, &Compiler::call, Prec::CALL);
    set(TokenType::DOT, nullptr, &Compiler::dot, Prec::CALL);
    set(TokenType::MINUS, &Compiler::unary, &Compiler::binary, Prec::TERM);
    set(TokenType::PLUS, nullptr, &Compiler::binary, Prec::TERM);
    set(TokenType::SLASH, nullptr, &Compiler::binary, Prec::FACTOR);
    set(TokenType::STAR, nullptr, &Compiler::binary, Prec::FACTOR);
    set(TokenType::BANG, &Compiler::unary, nullptr, Prec::NONE);
    set(TokenType::BANG_EQUAL, nullptr, &Compiler::binary, Prec::EQUALITY);
    set(TokenType::EQUAL_EQUAL, nullptr, &Compiler::binary, Prec::EQUALITY);
    set(TokenType::GREATER, nullptr, &Compiler::binary, Prec::COMPARISON);
    set(TokenType::GREATER_EQUAL, nullptr, &Compiler::binary, Prec::COMPARISON);
    set(TokenType::LESS, nullptr, &Compiler::binary, Prec::COMPARISON);
    set(TokenType::LESS_EQUAL, nullptr, &Compiler::binary, Prec::COMPARISON);
    set(TokenType::APPROX_EQUAL, nullptr, &Compiler::binary, Prec::COMPARISON);
    set(TokenType::IDENTIFIER, &Compiler::variable, nullptr, Prec::NONE);
    set(TokenType::STRING, &Compiler::stringy, nullptr, Prec::NONE);
    set(TokenType::NUMBER, &Compiler::number, nullptr, Prec::NONE);
    set(TokenType::IMAGINARY, &Compiler::imaginary, nullptr, Prec::NONE);
    set(TokenType::AND, nullptr, &Compiler::and_, Prec::AND);
    set(TokenType::ELSE, nullptr, &Compiler::else_, Prec::ASSIGNMENT);
    set(TokenType::FALSE, &Compiler::literal, nullptr, Prec::NONE);
    set(TokenType::NIL, &Compiler::literal, nullptr, Prec::NONE);
    set(TokenType::OR, nullptr, &Compiler::or_, Prec::OR);
    set(TokenType::SUPER, &Compiler::super_, nullptr, Prec::NONE);
    set(TokenType::THIS, &Compiler::this_, nullptr, Prec::NONE);
    set(TokenType::TRUE, &Compiler::literal, nullptr, Prec::NONE);
    set(TokenType::LEFT_SQUARE, &Compiler::arrayLit, &Compiler::subscript, Prec::CALL);
    set(TokenType::BACKSLASH, &Compiler::lambda, nullptr, Prec::NONE);

    return result;
}

constinit const Rules Compiler::rules = RulesMaker::make();
