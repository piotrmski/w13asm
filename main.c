#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "program-input/program-input.h"
#include "assembler/assembler.h"
#include "common/exit-code.h"

int main(int argc, const char * argv[]) {
    struct ProgramInput input = getProgramInput(argc, argv);

    FILE* asmFile = fopen(input.asmFilePath, "rb");

    if (asmFile == NULL) {
        printf("Error: could not read file \"%s\".\n", input.asmFilePath);
        exit(ExitCodeCouldNotReadAsmFile);
    }

    unsigned short* programMemory = assemble(asmFile);

    fclose(asmFile);

    FILE* binFile = fopen(input.binaryFilePath, "wb");

    if (binFile == NULL) {
        printf("Error: could not write to file \"%s\".\n", input.binaryFilePath);
        exit(ExitCodeCouldNotWriteBinFile);
    }

    int programSize = 0;

    for (int i = 0; i < PROGRAM_MEMORY_SIZE; ++i) {
        if (programMemory[i] != 0) {
            programSize = i + 1;
        }
    }

    if (programSize == 0) {
        printf("Error: the resulting program is empty.\n");
        exit(ExitCodeResultProgramEmpty);
    }

    fwrite(programMemory, sizeof(unsigned short), programSize, binFile);

    fclose(binFile);

    // TODO create the symbols file

    return ExitCodeSuccess;
}