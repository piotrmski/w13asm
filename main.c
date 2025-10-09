#include <stdio.h>
#include <string.h>
#include "program-input/program-input.h"
#include "tokenizer/tokenizer.h"

int main(int argc, const char * argv[]) {
    struct ProgramInput input = getProgramInput(argc, argv);

    if (input.error) { return 1; }

    if (input.asmFilePath == NULL || input.binaryFilePath == NULL) { return 0; }

    FILE* asmFile = fopen(input.asmFilePath, "r");

    if (asmFile == NULL) {
        printf("Error: could not read file \"%s\"\n.", input.binaryFilePath);
        return 1;
    }

    struct Token token = getInitialTokenState();

    do {
        getToken(&token, asmFile);
        printf("%d\t%s\t%d\n", token.lineNumber, token.stringValue, token.numberValue);
    } while (token.type != TokenTypeNone && token.type != TokenTypeError);

    fclose(asmFile);

    return 0;
}