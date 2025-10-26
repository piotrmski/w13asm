#ifndef tokenizer
#define tokenizer

#include <stdbool.h>
#include <stdio.h>

#define MAX_NAME_LEN_INCL_0 0x20
#define MAX_STR_VAL_LEN_INCL_0 0x2000  

enum TokenType {
    TokenTypeNone,
    TokenTypeLabelDefinition,
    TokenTypeLabelUseOrInstruction,
    TokenTypeDirective,
    TokenTypeDecimalNumber,
    TokenTypeHexNumber,
    TokenTypeOctalNumber,
    TokenTypeBinaryNumber,
    TokenTypeZTString,
    TokenTypeNZTString,
    _TokenTypeComment,
    _TokenTypeMinus,
    _TokenTypeZero
};

struct Token {
    int lineNumber;
    enum TokenType type;
    int numberValue;
    char* stringValue;
};

struct Token getInitialTokenState();

void getToken(struct Token* token, FILE* filePtr);


#endif