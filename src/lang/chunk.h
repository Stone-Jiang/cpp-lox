#pragma once
#include "commons.h"
#include "value.h"

namespace OpCode
{
enum Op: u8
{
    CONSTANT, NIL, TRUE, FALSE,
    POP, POP_UNHANDLED, GET_LOCAL, SET_LOCAL, GET_GLOBAL, DEFINE_GLOBAL, SET_GLOBAL,
    GET_UPVALUE, SET_UPVALUE,
    GET_PROPERTY, SET_PROPERTY, GET_SUPER,
    EQUAL, GREATER, LESS, APPROX, ADD, SUBTRACT, MULTIPLY, DIVIDE, NOT, NEGATE,
    IS_ERROR,
    PRINT, JUMP, JUMP_IF_FALSE, LOOP, 
    CALL,INVOKE, SUPER_INVOKE,
    CLOSURE, CLOSE_UPVALUE, RETURN, FAIL,
    CLASS, INHERIT, METHOD, STATIC_METHOD,
    MAKE_ARRAY, GET_INDEX, SET_INDEX, ARITH_SEQ
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
