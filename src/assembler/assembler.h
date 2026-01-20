#ifndef assembler
#define assembler
#include <stdio.h>
#include "../shared/shared.h"
#include "../tokenizer/tokenizer.h"

#define ADDRESS_SPACE_SIZE 0x2000

enum DataType {
    DataTypeNone = 0,
    DataTypeInstruction,
    DataTypeChar,
    DataTypeInt
};

struct AssemblerResult {
    unsigned char programMemory[ADDRESS_SPACE_SIZE];
    enum DataType dataType[ADDRESS_SPACE_SIZE];
    struct Token labelsByAddress[ADDRESS_SPACE_SIZE];
};

struct AssemblerResult assemble(struct Token* tokens);

#endif