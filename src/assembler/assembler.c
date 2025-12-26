#include "assembler.h"
#include "../tokenizer/tokenizer.h"
#include <stdbool.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <limits.h>
#include "../../common/exit-code.h"

#define MAX_LABEL_DEFS 0x800
#define MAX_LABEL_USES 0x2000
#define MAX_LABEL_NAME_LEN_INCL_0 0x20

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

enum Directive {
    DirectiveOrg,
    DirectiveAlign,
    DirectiveFill,
    DirectiveLsb,
    DirectiveMsb,
    DirectiveInvalid
};

struct LabelDefinition {
    char* name;
    int lineNumber;
    int address;
};

struct LabelUse {
    char* name;
    int offset;
    int byte;
    int lineNumber;
    int address;
};

struct LabelUseParseResult {
    char* name;
    int offset;
};

struct AssemblerState {
    char* source;
    int lineNumber;
    int currentAddress;
    bool programMemoryWritten[ADDRESS_SPACE_SIZE];
    struct LabelDefinition labelDefinitions[MAX_LABEL_DEFS];
    int labelDefinitionsCount;
    struct LabelUse labelUses[MAX_LABEL_USES];
    int labelUsesCount;
    struct AssemblerResult result;
};

static void assertNoMemoryViolation(struct AssemblerState* state, int address, int lineNumber) {
    if (address < 0 || address >= ADDRESS_SPACE_SIZE) {
        printf("Error on line %d: attempting to declare memory value outside of address space.\n", lineNumber);
        exit(ExitCodeDeclaringValueOutOfMemoryRange);
    }

    if (state->programMemoryWritten[address]) {
        printf("Error on line %d: attempting to override memory value.\n", lineNumber);
        exit(ExitCodeMemoryValueOverridden);
    }
}

// static void assertNumberLiteralInRange(struct Token* token) {
//     if (token->numberValue < CHAR_MIN ||
//         token->numberValue > UCHAR_MAX) {
//         printf("Error on line %d: number %d is out of range for a number literal.\n", token->lineNumber, token->numberValue);
//         exit(ExitCodeNumberLiteralOutOutRange);
//     }
// }

static char charUppercase(char ch) {
    if (ch >= 'a' && ch <= 'z') return ch - 0x20;
    else return ch;
}

static bool stringsEqualCaseInsensitive(char* string1, char* string2) {
    for (int i = 0;; ++i) {
        if (charUppercase(string1[i]) != charUppercase(string2[i])) {
            return false;
        }

        if (string1[i] == 0 || string2[i] == 0) {
            return true;
        }
    }
}

static enum Instruction getInstruction(char* name) {
    if (stringsEqualCaseInsensitive(name, "LD")) {
        return InstructionLd;
    } else if (stringsEqualCaseInsensitive(name, "NOT")) {
        return InstructionNot;
    } else if (stringsEqualCaseInsensitive(name, "ADD")) {
        return InstructionAdd;
    } else if (stringsEqualCaseInsensitive(name, "AND")) {
        return InstructionAnd;
    } else if (stringsEqualCaseInsensitive(name, "ST")) {
        return InstructionSt;
    } else if (stringsEqualCaseInsensitive(name, "JMP")) {
        return InstructionJmp;
    } else if (stringsEqualCaseInsensitive(name, "JMN")) {
        return InstructionJmn;
    } else if (stringsEqualCaseInsensitive(name, "JMZ")) {
        return InstructionJmz;
    } else {
        return InstructionInvalid;
    }
}

static enum Directive getDirective(char* name) {
    if (stringsEqualCaseInsensitive(name, ".ORG")) {
        return DirectiveOrg;
    } else if (stringsEqualCaseInsensitive(name, ".ALIGN")) {
        return DirectiveAlign;
    } else if (stringsEqualCaseInsensitive(name, ".FILL")) {
        return DirectiveFill;
    } else if (stringsEqualCaseInsensitive(name, ".LSB")) {
        return DirectiveLsb;
    } else if (stringsEqualCaseInsensitive(name, ".MSB")) {
        return DirectiveMsb;
    } else {
        return DirectiveInvalid;
    }
}

static bool isStringLiteral(char* tokenValue) {
    return tokenValue[0] = '"';
}

static bool isNumberLiteral(char* tokenValue) {
    return tokenValue[0] >= '0' && tokenValue[0] <= '9' || tokenValue[0] == '-' && tokenValue[0] >= '0' && tokenValue[0] <= '9';
}

static bool isCharacterLiteral(char* tokenValue) {
    return tokenValue[0] == '\'' || tokenValue[0] == '-' && tokenValue[1] == '\'';
}


// static void parseToken(struct Token* token, struct AssemblerState* state, struct AssemblerResult* result, FILE* filePtr) {
//     switch (token->type) {
//         case TokenTypeNone:
//             break;
//         case TokenTypeLabelDefinition:
//             if (state->labelDefinitionsIndex >= MAX_LABELS) {
//                 printf("Error on line %d: too many labels.\n", token->lineNumber);
//                 exit(ExitCodeTooManyLabels);
//             }
//             state->labelDefinitions[state->labelDefinitionsIndex].name = token->stringValue;
//             state->labelDefinitions[state->labelDefinitionsIndex].lineNumber = token->lineNumber;
//             state->labelDefinitions[state->labelDefinitionsIndex++].address = state->address;
//             state->lastTokenWasLabelDefinition = true;
//             for (int i = 0; i < state->labelDefinitionsIndex - 1; ++i) {
//                 if (strcmp(state->labelDefinitions[i].name, token->stringValue) == 0) {
//                     printf("Error on line %d: label name \"%s\" is not unique.\n", token->lineNumber, token->stringValue);
//                     exit(ExitCodeLabelNameNotUnique);
//                 }
//             }
//             break;
//         case TokenTypeLabelUseOrInstruction:
//             enum Instruction instruction = getInstruction(token->stringValue);
//             if (instruction == InstructionInvalid) {
//                 printf("Error on line %d: invalid instruction \"%s\".\n", token->lineNumber, token->stringValue);
//                 exit(ExitCodeInvalidInstruction);
//             } else {
//                 assertNoMemoryViolation(state, token, state->address);
//                 assertNoMemoryViolation(state, token, state->address + 1);
//                 unsigned short opcode = (char)instruction << 13;
//                 result->dataType[state->address] = DataTypeInstruction;
//                 state->programMemoryWritten[state->address] = true;
//                 state->programMemoryWritten[state->address + 1] = true;
//                 getToken(token, filePtr);
//                 switch (token->type) {
//                     case TokenTypeDecimalNumber:
//                     case TokenTypeHexNumber:
//                     case TokenTypeOctalNumber:
//                     case TokenTypeBinaryNumber:
//                         if (token->numberValue < 0 || token->numberValue >= ADDRESS_SPACE_SIZE) {
//                             printf("Error on line %d: attempting to reference invalid address 0x%04X.\n", token->lineNumber, token->numberValue);
//                             exit(ExitCodeReferenceToInvalidAddress);
//                         }
//                         opcode |= token->numberValue;
//                         break;
//                     case TokenTypeLabelUseOrInstruction:
//                         if (state->labelUsesIndex >= MAX_LABELS) {
//                             printf("Error on line %d: too many labels.\n", token->lineNumber);
//                             exit(ExitCodeTooManyLabels);
//                         }
//                         state->labelUses[state->labelUsesIndex].name = token->stringValue;
//                         state->labelUses[state->labelUsesIndex].lineNumber = token->lineNumber;
//                         state->labelUses[state->labelUsesIndex++].address = state->address;
//                         break;
//                     default:
//                         printf("Error on line %d: unexpected token following instruction name.\n", token->lineNumber);
//                         exit(ExitCodeUnexpectedTokenAfterInstruction);
//                 }
//                 result->programMemory[state->address] = opcode;
//                 result->programMemory[state->address + 1] = opcode >> 8;
//                 state->address += 2;
//             }
//             state->lastTokenWasLabelDefinition = false;
//             break;
//         case TokenTypeDirective:
//             enum Directive directive = getDirective(token->stringValue);
//             switch (directive) {
//                 case DirectiveOrg:
//                     getToken(token, filePtr);
//                     switch (token->type) {
//                         case TokenTypeDecimalNumber:
//                         case TokenTypeHexNumber:
//                         case TokenTypeOctalNumber:
//                         case TokenTypeBinaryNumber:
//                             if (token->numberValue < 0 || token->numberValue >= ADDRESS_SPACE_SIZE) {
//                                 printf("Error on line %d: attempting to set origin to invalid address 0x%04X.\n", token->lineNumber, token->numberValue);
//                                 exit(ExitCodeOriginOutOfMemoryRange);
//                             }
//                             state->address = token->numberValue;
//                             if (state->lastTokenWasLabelDefinition) {
//                                 state->labelDefinitions[state->labelDefinitionsIndex - 1].address = state->address;
//                             }
//                             break;
//                         default:
//                             printf("Error on line %d: unexpected token following .ORG.\n", token->lineNumber);
//                             exit(ExitCodeUnexpectedTokenAfterOrg);
//                     }
//                     break;
//                 case DirectiveAlign:
//                     getToken(token, filePtr);
//                     switch (token->type) {
//                         case TokenTypeDecimalNumber:
//                         case TokenTypeHexNumber:
//                         case TokenTypeOctalNumber:
//                         case TokenTypeBinaryNumber:
//                             if (token->numberValue < 1 || token->numberValue > 12) {
//                                 printf("Error on line %d: invalid align parameter \"%d\". Must be between 1 and 12.\n", token->lineNumber, token->numberValue);
//                                 exit(ExitCodeInvalidAlignParameter);
//                             }
//                             unsigned short bitsToReset = (1 << token->numberValue) - 1;
//                             state->address = (state->address & bitsToReset) == 0
//                                 ? state->address
//                                 : ((state->address & ~bitsToReset) + bitsToReset + 1);
//                             if (state->address >= ADDRESS_SPACE_SIZE) {
//                                 printf("Error on line %d: attempting to set origin via align to invalid address 0x%04X.\n", token->lineNumber, state->address);
//                                 exit(ExitCodeOriginOutOfMemoryRange);
//                             }
//                             if (state->lastTokenWasLabelDefinition) {
//                                 state->labelDefinitions[state->labelDefinitionsIndex - 1].address = state->address;
//                             }
//                             break;
//                         default:
//                             printf("Error on line %d: unexpected token following .ALIGN.\n", token->lineNumber);
//                             exit(ExitCodeUnexpectedTokenAfterOrg);
//                     }
//                     break;
//                 case DirectiveFill:
//                     unsigned char valueToFill;
//                     unsigned char fillCount;
//                     enum DataType valueToFillType;
//                     getToken(token, filePtr);
//                     switch (token->type) {
//                         case TokenTypeDecimalNumber:
//                         case TokenTypeHexNumber:
//                         case TokenTypeOctalNumber:
//                         case TokenTypeBinaryNumber:
//                             assertNumberLiteralInRange(token);
//                             valueToFill = token->numberValue;
//                             valueToFillType = DataTypeInt;
//                             break;
//                         case TokenTypeNZTString:
//                             if (token->stringValue[0] == 0 || token->stringValue[1] != 0) {
//                                 printf("Error on line %d: expected no more or less than one character.\n", token->lineNumber);
//                                 exit(ExitCodeFillValueStringNotAChar);
//                             }
//                             valueToFill = token->stringValue[0];
//                             valueToFillType = DataTypeChar;
//                             break;
//                         default:
//                             printf("Error on line %d: unexpected token following .FILL.\n", token->lineNumber);
//                             exit(ExitCodeUnexpectedTokenAfterFill);
//                     }
//                     getToken(token, filePtr);
//                     switch (token->type) {
//                         case TokenTypeDecimalNumber:
//                         case TokenTypeHexNumber:
//                         case TokenTypeOctalNumber:
//                         case TokenTypeBinaryNumber:
//                             if (token->numberValue < 1) {
//                                 printf("Error on line %d: fill count must be positive.\n", token->lineNumber);
//                                 exit(ExitCodeFillCountNotPositive);
//                             }
//                             fillCount = token->numberValue;
//                             break;
//                         default:
//                             printf("Error on line %d: unexpected token following .FILL value.\n", token->lineNumber);
//                             exit(ExitCodeUnexpectedTokenAfterFill);
//                     }
//                     for (int i = 0; i < fillCount; ++i) {
//                         assertNoMemoryViolation(state, token, state->address);
//                         result->programMemory[state->address] = valueToFill;
//                         result->dataType[state->address] = valueToFillType;
//                         state->programMemoryWritten[state->address++] = true;
//                     }
//                     break;
//                 default:
//                     printf("Error on line %d: invalid directive \"%s\".\n", token->lineNumber, token->stringValue);
//                     exit(ExitCodeInvalidDirective);
//             }
//             state->lastTokenWasLabelDefinition = false;
//             break;
//         case TokenTypeDecimalNumber:
//         case TokenTypeHexNumber:
//         case TokenTypeOctalNumber:
//         case TokenTypeBinaryNumber:
//             assertNoMemoryViolation(state, token, state->address);
//             assertNumberLiteralInRange(token);
//             result->programMemory[state->address] = token->numberValue;
//             result->dataType[state->address] = DataTypeInt;
//             state->programMemoryWritten[state->address++] = true;
//             state->lastTokenWasLabelDefinition = false;
//             break;
//         case TokenTypeZTString:
//         case TokenTypeNZTString:
//             assertNoMemoryViolation(state, token, state->address);
//             result->dataType[state->address] = DataTypeChar;
//             char* strptr = token->stringValue;
//             while(*strptr != 0) {
//                 assertNoMemoryViolation(state, token, state->address);
//                 result->programMemory[state->address] = *strptr;
//                 result->dataType[state->address] = DataTypeChar;
//                 state->programMemoryWritten[state->address++] = true;
//                 ++strptr;
//             }
//             if (token->type == TokenTypeZTString) {
//                 assertNoMemoryViolation(state, token, state->address);
//                 result->programMemory[state->address] = 0;
//                 result->dataType[state->address] = DataTypeChar;
//                 state->programMemoryWritten[state->address++] = true;
//             }
//             state->lastTokenWasLabelDefinition = false;
//             break;
//         default:
//             printf("Error on line %d: invalid token \"%s\".\n", token->lineNumber, token->stringValue);
//             exit(ExitCodeInvalidToken);
//     }
// }

// struct LabelInfo* findLabelDefinition(struct AssemblerState* state, struct LabelInfo* labelUse) {
//     for (int i = 0; i < state->labelDefinitionsIndex; ++i) {
//         if (strcmp(state->labelDefinitions[i].name, labelUse->name) == 0) {
//             return &state->labelDefinitions[i];
//         }
//     }

//     printf("Error on line %d: label \"%s\" is undefined.\n", labelUse->lineNumber, labelUse->name);
//     exit(ExitCodeUndefinedLabel);
// }

static struct Token getNextToken(struct AssemblerState* state) {
    return getToken(&state->source, &state->lineNumber);
}

static bool isValidLabelDefinitionRemoveColon(struct Token token, struct AssemblerState* state) {
    if (token.value[--token.length] != ':') {
        return false;
    }

    token.value[token.length] = 0; // Trim the trailing colon character

    if (token.length > MAX_LABEL_NAME_LEN_INCL_0 - 1) {
        printf("Error on line %d: label name too long.\n", token.lineNumber);
        exit(ExitCodeLabelNameTooLong);
    }

    for (int i = 0; i < token.length; ++i) {
        char ch = token.value[i];
        bool characterValid = ch == '_' || ch >= 'a' && ch <= 'z' || ch >= 'A' && ch <= 'Z' || i > 0 && ch >= '0' && ch <= 9;
        if (!characterValid) {
            printf("Error on line %d: \"%s\" is not a valid label name.\n", token.lineNumber, token.value);
            exit(ExitCodeInvalidLabelName);
        }
    }

    for (int i = 0; i < state->labelDefinitionsCount; ++i) {
        if (strcmp(state->labelDefinitions[i].name, token.value) == 0) {
            printf("Error on line %d: label name \"%s\" is not unique.\n", token.lineNumber, token.value);
            exit(ExitCodeLabelNameNotUnique);
        }
    }

    return true;
}

static int parseNumberLiteral(struct Token token) {
    char* endChar;
    int result = strtol(token.value, &endChar, 0);
    if (*endChar != 0) {
        printf("Error on line %d: \"%s\" is not a valid number.\n", token.lineNumber, token.value);
        exit(ExitCodeInvalidNumberLiteral);
    }
    return result;
}

static struct LabelUseParseResult parseLabelUse(struct Token token) {
    char* offsetSign = strpbrk(token.value, "+-");
    int offset;
    if (offsetSign != NULL) {
        offset = parseNumberLiteral((struct Token) { token.lineNumber, 0, offsetSign });
        *offsetSign = 0;
    }
    return (struct LabelUseParseResult) { token.value, offset };
}

static void insertInstruction(struct AssemblerState* state, enum Instruction instruction) {
    assertNoMemoryViolation(state, state->currentAddress, state->lineNumber);
    assertNoMemoryViolation(state, state->currentAddress + 1, state->lineNumber);
    state->programMemoryWritten[state->currentAddress] = true;
    state->programMemoryWritten[state->currentAddress + 1] = true;
    state->result.dataType[state->currentAddress] = DataTypeInstruction;

    unsigned short instructionCode = instruction << 13;
    struct Token param = getNextToken(state);

    if (isNumberLiteral(param.value)) {
        int paramValue = parseNumberLiteral(param);
        if (paramValue < 0 || paramValue >= ADDRESS_SPACE_SIZE) {
            printf("Error on line %d: attempting to reference invalid address 0x%04X.\n", param.lineNumber, paramValue);
            exit(ExitCodeReferenceToInvalidAddress);
        } 
        instructionCode |= paramValue;
    } else {
        struct LabelUseParseResult labelUse = parseLabelUse(param);
        state->labelUses[state->labelUsesCount++] =
            (struct LabelUse) { labelUse.name, labelUse.offset, 0, param.lineNumber, state->currentAddress };
        state->labelUses[state->labelUsesCount++] =
            (struct LabelUse) { labelUse.name, labelUse.offset, 1, param.lineNumber, state->currentAddress + 1 };
    }
    state->result.programMemory[state->currentAddress++] = instructionCode;
    state->result.programMemory[state->currentAddress++] = instructionCode >> 8;
}

static void applyDirective(struct AssemblerState* state, enum Directive directive) {
    // TODO
}

static void declareString(struct AssemblerState* state, struct Token token) {
    // TODO
}

static void declareNumber(struct AssemblerState* state, struct Token token) {
    assertNoMemoryViolation(state, state->currentAddress, state->lineNumber);
    state->programMemoryWritten[state->currentAddress] = true;
    state->result.dataType[state->currentAddress] = DataTypeInt;
    int number = parseNumberLiteral(token);
    if (number < CHAR_MIN || number > UCHAR_MAX) {
        printf("Error on line %d: number %d is out of range.\n", token.lineNumber, number);
        exit(ExitCodeNumberLiteralOutOutRange);
    }
    state->result.programMemory[state->currentAddress++] = number;
}

static void declareCharacter(struct AssemblerState* state, struct Token token) {
    // TODO
}

static struct Token parseLabelDefinitionsGetNextToken(struct AssemblerState* state) {
    struct Token token;

    while (true) {
        token = getNextToken(state);
        if (isValidLabelDefinitionRemoveColon(token, state)) {
            if (state->labelDefinitionsCount == MAX_LABEL_DEFS - 1) {
                printf("Error on line %d: too many label definitions.\n", token.lineNumber);
                exit(ExitCodeTooManyLabelDefinitions);
            }
            state->labelDefinitions[state->labelDefinitionsCount++] = (struct LabelDefinition) { token.value, token.lineNumber, state->currentAddress };
        } else {
            break;
        }
    }

    return token;
}

/// Returns true if statement parsing should continue
static bool parseStatement(struct AssemblerState* state) {
    int labelDefinitionsStartIndex = state->labelDefinitionsCount;
    struct Token firstTokenAfterLabels = parseLabelDefinitionsGetNextToken(state);

    if (firstTokenAfterLabels.value == NULL) {
        if (state->labelDefinitionsCount > labelDefinitionsStartIndex) {
            printf("Error on line %d: unexpected label definition at the end of the file.\n", state->lineNumber);
            exit(ExitCodeUnexpectedLabelAtEndOfFile);
        }

        return false;
    }

    enum Instruction instruction;
    enum Directive directive;

    if ((instruction = getInstruction(firstTokenAfterLabels.value)) != InstructionInvalid) {
        insertInstruction(state, instruction);
    } else if ((directive = getDirective(firstTokenAfterLabels.value)) != DirectiveInvalid) {
        applyDirective(state, directive);
    } else if (isStringLiteral(firstTokenAfterLabels.value)) {
        declareString(state, firstTokenAfterLabels);
    } else if (isNumberLiteral(firstTokenAfterLabels.value)) {
        declareNumber(state, firstTokenAfterLabels);
    } else if (isCharacterLiteral(firstTokenAfterLabels.value)) {
        declareCharacter(state, firstTokenAfterLabels);
    } else {
        printf("Error on line %d: invalid token \"%s\".\n", firstTokenAfterLabels.lineNumber, firstTokenAfterLabels.value);
        exit(ExitCodeInvalidToken);
    }

    return true;
}

static void resolveLabels(struct AssemblerState* state) {
    // TODO
}

struct AssemblerResult assemble(char* source) {
    struct AssemblerState state = { source, 1, 0, { false }, { 0 }, 0, { 0 }, 0, (struct AssemblerResult){ { 0 }, { DataTypeNone }, { NULL } } };

    while (parseStatement(&state)) {}

    resolveLabels(&state);

    return state.result;
}