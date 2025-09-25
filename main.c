#include <stdio.h>
#include <string.h>
#include "program-input/program-input.h"


int main(int argc, const char * argv[]) {
    struct ProgramInput input = getProgramInput(argc, argv);

    if (input.error) { return 1; }

    if (input.asmFilePath == NULL || input.binaryFilePath == NULL) { return 0; }

    return 0;
}