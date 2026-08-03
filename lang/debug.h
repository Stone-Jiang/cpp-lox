#pragma once
#include "object.h"
#include "value.h"

class Debug
{
public:
    static void disassembleChunk(Chunk& chunk, const string& name);
    static int disassembleInstruction(Chunk& chunk, int offset);

private:
    static int simpleInstruction(const string& name, int offset);
    static int constantInstruction(const string& name, Chunk& chunk, int offset);
    static int invokeInstruction(const string& name, Chunk& chunk, int offset);
    static int byteInstruction(const string& name, Chunk& chunk, int offset);
    static int jumpInstruction(const string& name, int sign, Chunk& chunk, int offset);
};
