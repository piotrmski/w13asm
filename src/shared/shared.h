#ifndef shared
#define shared

#include <stdbool.h>
#include "../tokenizer/tokenizer.h"

#define MAX_LABEL_DEFS 0x2000
#define MAX_LABEL_NAME_LEN_INCL_0 0x20

struct Token {
    char* value;
    int length;
    int lineNumber;
    char* macroName;
    int macroInvocationIndex;
};

enum Instruction {
    InstructionLd = 0,
    InstructionNot = 1,
    InstructionAdd = 2,
    InstructionAnd = 3,
    InstructionSt = 4,
    InstructionJmp = 5,
    InstructionJmn = 6,
    InstructionJmz = 7,
    InstructionInvalid
};

char charUppercase(char ch);

bool stringsEqualCaseInsensitive(char* string1, char* string2);

const char* getInstructionName(enum Instruction instruction);

void assertTokenNotEmpty(struct Token token);

#endif