#include "chunk.h"

void Chunk::write(u8 byte, int line)
{
    code.push_back(byte);
    lines.push_back(line);
}

int Chunk::size() const
{
    return static_cast<int>(code.size());
}

int Chunk::addConst(Value val)
{
    constants.write(val);
    return constants.size()-1;
}

void Chunk::clear()
{
    code.clear();
    lines.clear();
    constants.values.clear();
}