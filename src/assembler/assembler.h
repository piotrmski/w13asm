#ifndef assembler
#define assembler
#include <stdio.h>
#include "../tokenizer/tokenizer.h"

#define PROGRAM_MEMORY_SIZE 0x1fff
#define ADDRESS_SPACE_SIZE 0x2000

enum DataType {
    DataTypeNone = 0,
    DataTypeInstruction,
    DataTypeChar,
    DataTypeInt
};

struct AssemblerResult {
    unsigned short programMemory[PROGRAM_MEMORY_SIZE];
    enum DataType dataType[PROGRAM_MEMORY_SIZE];
    char* labelNameByAddress[ADDRESS_SPACE_SIZE];
};

struct AssemblerResult assemble(FILE* filePtr);

#endif