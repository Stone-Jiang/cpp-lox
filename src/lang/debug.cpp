#include "debug.h"
#include "../utils/color.h"

void Debug::disassembleChunk(Chunk& chunk, const string& name)
{
    utils::TerminalColor<utils::Color::BrightGreen> color;
    printf("== %s ==\n", name.c_str());
    for (int offset = 0; offset < chunk.size();)
        offset = disassembleInstruction(chunk, offset);
}

int Debug::disassembleInstruction(Chunk& chunk, int offset)
{
    utils::TerminalColor<utils::Color::Green> color;
    utils::TerminalColor<utils::Color::Dim> style;
    printf("%04d ", offset);

    if (offset>0 && chunk.lines[offset]==chunk.lines[offset-1])
        printf("   | ");
    else
        printf("%4d ", chunk.lines[offset]);

    u8 ins = chunk.code[offset];
    switch (ins) 
    {
        case OpCode::CONSTANT:
            return constantInstruction("OpCode::CONSTANT", chunk, offset);
        case OpCode::NIL:
            return simpleInstruction("OpCode::NIL", offset);
        case OpCode::TRUE:
            return simpleInstruction("OpCode::TRUE", offset);
        case OpCode::FALSE:
            return simpleInstruction("OpCode::FALSE", offset);
        case OpCode::POP:
            return simpleInstruction("OpCode::POP", offset);
        case OpCode::POP_UNHANDLED:
            return simpleInstruction("OpCode::POP_UNHANDLED", offset);
        case OpCode::GET_LOCAL:
            return byteInstruction("OpCode::GET_LOCAL", chunk, offset);
        case OpCode::SET_LOCAL:
            return byteInstruction("OpCode::SET_LOCAL", chunk, offset);
        case OpCode::GET_GLOBAL:
            return constantInstruction("OpCode::GET_GLOBAL", chunk, offset);
        case OpCode::DEFINE_GLOBAL:
            return constantInstruction("OpCode::DEFINE_GLOBAL", chunk, offset);
        case OpCode::SET_GLOBAL:
            return constantInstruction("OpCode::SET_GLOBAL", chunk, offset);
        case OpCode::GET_UPVALUE:
            return byteInstruction("OpCode::GET_UPVALUE", chunk, offset);
        case OpCode::SET_UPVALUE:
            return byteInstruction("OpCode::SET_UPVALUE", chunk, offset);
        case OpCode::GET_PROPERTY:
            return constantInstruction("OpCode::GET_PROPERTY", chunk, offset);
        case OpCode::SET_PROPERTY:
            return constantInstruction("OpCode::SET_PROPERTY", chunk, offset);
        case OpCode::GET_SUPER:
            return constantInstruction("OpCode::GET_SUPER", chunk, offset);
        case OpCode::EQUAL:
        return simpleInstruction("OpCode::EQUAL", offset);
            case OpCode::GREATER:
        return simpleInstruction("OpCode::GREATER", offset);
            case OpCode::LESS:
        return simpleInstruction("OpCode::LESS", offset);
            case OpCode::ADD:
        return simpleInstruction("OpCode::ADD", offset);
            case OpCode::SUBTRACT:
        return simpleInstruction("OpCode::SUBTRACT", offset);
            case OpCode::MULTIPLY:
        return simpleInstruction("OpCode::MULTIPLY", offset);
            case OpCode::DIVIDE:
        return simpleInstruction("OpCode::DIVIDE", offset);
            case OpCode::NOT:
        return simpleInstruction("OpCode::NOT", offset);
        case OpCode::NEGATE:
        return simpleInstruction("OpCode::NEGATE", offset);
        case OpCode::IS_ERROR:
            return simpleInstruction("OpCode::IS_ERROR", offset);

        case OpCode::PRINT:
            return simpleInstruction("OpCode::PRINT", offset);

        case OpCode::JUMP:
            return jumpInstruction("OpCode::JUMP", 1, chunk, offset);
        case OpCode::JUMP_IF_FALSE:
            return jumpInstruction("OpCode::JUMP_IF_FALSE", 1, chunk, offset);

        case OpCode::LOOP:
            return jumpInstruction("OpCode::LOOP", -1, chunk, offset);

        case OpCode::CALL:
            return byteInstruction("OpCode::CALL", chunk, offset);

        case OpCode::INVOKE:
            return invokeInstruction("OpCode::INVOKE", chunk, offset);

        case OpCode::SUPER_INVOKE:
            return invokeInstruction("OpCode::SUPER_INVOKE", chunk, offset);

        case OpCode::CLOSURE: {
            offset++;
            uint8_t constant = chunk.code[offset++];
            printf("%-16s %4d ", "OpCode::CLOSURE", constant);
            printValue(chunk.constants.values[constant]);
            printf("\n");

            auto param = chunk.constants.values[constant];
            auto function = static_cast<ObjFunction*>(param.as_obj());
            for (int j = 0; j < function->upvalCount; j++) 
            {
                int isLocal = chunk.code[offset++];
                int index = chunk.code[offset++];
                printf("%04d      |                     %s %d\n",
                    offset - 2, isLocal ? "local" : "upvalue", index);
            }
            return offset;
        }

        case OpCode::CLOSE_UPVALUE:
            return simpleInstruction("OpCode::CLOSE_UPVALUE", offset);
        case OpCode::RETURN:
            return simpleInstruction("OpCode::RETURN", offset);
        case OpCode::FAIL:
            return simpleInstruction("OpCode::FAIL", offset);
        case OpCode::CLASS:
            return constantInstruction("OpCode::CLASS", chunk, offset);
        case OpCode::INHERIT:
            return simpleInstruction("OpCode::INHERIT", offset);
        case OpCode::METHOD:
            return constantInstruction("OpCode::METHOD", chunk, offset);
        case OpCode::STATIC_METHOD:
            return constantInstruction("OpCode::STATIC_METHOD", chunk, offset);

        case OpCode::MAKE_ARRAY:
            return byteInstruction("OpCode::MAKE_ARRAY", chunk, offset);
        case OpCode::GET_INDEX:
            return simpleInstruction("OpCode::GET_INDEX", offset);
        case OpCode::SET_INDEX:
            return simpleInstruction("OpCode::SET_INDEX", offset);
        case OpCode::EXTEND:
            return simpleInstruction("OpCode::EXTEND", offset);
        case OpCode::ITER_SNAP:
            return byteInstruction("OpCode::ITER_SNAP", chunk, offset);
        case OpCode::NESTED_CLASS:
            return nestedClassInstruction(chunk, offset);
        default:
            printf("Unknown opcode %d\n", ins);
            return offset + 1;
    }
}

int Debug::simpleInstruction(const string& name, int offset)
{
    printf("%s\n", name.c_str());
    return offset + 1;
}

int Debug::constantInstruction(const string& name, Chunk& chunk, int offset) 
{
    if (offset + 1 >= chunk.size()) 
    {
        printf("%-16s ERROR: missing constant operand\n", name.c_str());
        return offset + 2;
    }
    uint8_t constant = chunk.code[offset + 1];
    printf("%-16s %4d '", name.c_str(), constant);
    printValue(chunk.constants.values[constant]);
    printf("'\n");
    return offset+2;
}

int Debug::invokeInstruction(const string& name, Chunk& chunk, int offset)
{
    u8 constant = chunk.code[offset+1];
    u8 argCount = chunk.code[offset+2];
    printf("%-16s (%d args) %4d '", name.c_str(), argCount, constant);
    printValue(chunk.constants.values[constant]);
    printf("'\n");
    return offset + 3;
}

int Debug::byteInstruction(const string& name, Chunk& chunk, int offset) 
{
    u8 slot = chunk.code[offset + 1];
    printf("%-16s %4d\n", name.c_str(), slot);
    return offset + 2;
}

int Debug::jumpInstruction(const string& name, int sign, Chunk& chunk, int offset) 
{
    u16 jump = (uint16_t)(chunk.code[offset+1] << 8);
    jump |= chunk.code[offset+2];
    printf("%-16s %4d -> %d\n", name.c_str(), offset,
            offset + 3 + sign * jump);
    return offset + 3;
}

int Debug::nestedClassInstruction(Chunk& chunk, int offset)
{
    const u8 constant = chunk.code[offset+1];
    const u8 hasSuper = chunk.code[offset+2];
    printf("%-24s %4d '", "NESTED_CLASS", constant);
    printValue(chunk.constants[constant]);
    printf("' superclass=%s\n", hasSuper ? "yes" : "no");
    return offset + 3;
}