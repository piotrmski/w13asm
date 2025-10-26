#ifndef program_input
#define program_input

#include <stdbool.h>

struct ProgramInput {
    const char* asmFilePath;
    const char* binaryFilePath;
    const char* symbolsFilePath;
};

struct ProgramInput getProgramInput(int argc, const char * argv[]);

#endif