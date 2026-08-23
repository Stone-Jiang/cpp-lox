#pragma once
#include "commons.h"
#include "value.h"

namespace OpCode
{
enum Op: u8
{
    CONSTANT, NIL, TRUE, FALSE,
    POP, GET_LOCAL, SET_LOCAL, GET_GLOBAL, DEFINE_GLOBAL, SET_GLOBAL,
    GET_UPVALUE, SET_UPVALUE,
    GET_PROPERTY, SET_PROPERTY, GET_SUPER,
    EQUAL, GREATER, LESS, ADD, SUBTRACT, MULTIPLY, DIVIDE, NOT, NEGATE,
    PRINT, JUMP, JUMP_IF_FALSE, LOOP, 
    CALL,INVOKE, SUPER_INVOKE,
    CLOSURE, CLOSE_UPVALUE, RETURN,
    CLASS, INHERIT, METHOD, STATIC_METHOD
};
}


class Chunk
{
    friend class Debug;
    friend class VM;
    friend class Compiler;
private: 
    Vector<u8> code;
    Vector<int> lines;
    ValueArray constants;
public:
    explicit Chunk(VM* owner = nullptr): code(owner), lines(owner), constants(owner) {}
    ~Chunk() = default;

    void write(u8 byte, int line);
    int size() const;
    int addConst(Value val);
    void clear();
};
