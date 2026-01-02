#include "assembler.h"
#include "../tokenizer/tokenizer.h"
#include "../../common/exit-code.h"
#include <stdbool.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <limits.h>
#include <errno.h>

#define MAX_LABEL_DEFS 0x1000
#define MAX_LABEL_USES 0x1000
#define MAX_IMMEDIATE_VAL_USES 0x1000
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

struct ImmediateValueUse {
    char* stringValue;
    int lineNumber;
    int address;
};

struct LabelUseParseResult {
    char* name;
    int offset;
};

struct EscapeSequenceParseResult {
    char character;
    int length;
};

static char* sourceString;
static int lineNumber = 1;
static int currentAddress = 0;
static bool programMemoryWritten[ADDRESS_SPACE_SIZE] = { false };
static struct LabelDefinition labelDefinitions[MAX_LABEL_DEFS];
static int labelDefinitionsCount = 0;
static struct LabelUse labelUses[MAX_LABEL_USES];
static int labelUsesCount = 0;
static struct ImmediateValueUse immediateValueUses[MAX_IMMEDIATE_VAL_USES];
static int immediateValueUsesCount = 0;
static struct AssemblerResult result = (struct AssemblerResult){ { 0 }, { DataTypeNone }, { NULL } };

static void assertNoMemoryViolation(int address, int lineNumber) {
    if (address < 0 || address >= ADDRESS_SPACE_SIZE) {
        printf("Error on line %d: attempting to declare memory value outside of address space.\n", lineNumber);
        exit(ExitCodeDeclaringValueOutOfMemoryRange);
    }

    if (programMemoryWritten[address]) {
        printf("Error on line %d: attempting to override memory value.\n", lineNumber);
        exit(ExitCodeMemoryValueOverridden);
    }

    programMemoryWritten[address] = true;
}

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

static const char* getInstructionName(enum Instruction instruction) {
    switch (instruction) {
        case InstructionLd: return "LD"; 
        case InstructionNot: return "NOT"; 
        case InstructionAdd: return "ADD"; 
        case InstructionAnd: return "AND"; 
        case InstructionSt: return "ST"; 
        case InstructionJmp: return "JMP"; 
        case InstructionJmn: return "JMN"; 
        case InstructionJmz: return "JMZ"; 
        case InstructionInvalid: return "";
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
    return tokenValue[0] == '"';
}

static bool isNumberLiteral(char* tokenValue) {
    return tokenValue[0] >= '0' && tokenValue[0] <= '9' || tokenValue[0] == '-' && tokenValue[1] >= '0' && tokenValue[1] <= '9';
}

static bool isCharacterLiteral(char* tokenValue) {
    return tokenValue[0] == '\'' || tokenValue[0] == '-' && tokenValue[1] == '\'';
}

static bool isImmediateValue(char* tokenValue) {
    return tokenValue[0] == '#';
}

static bool instructionAcceptsImmediateValue(enum Instruction instruction) {
    return instruction < InstructionSt;
}

struct LabelDefinition* findLabelDefinition(struct LabelUse* labelUse) {
    for (int i = 0; i < labelDefinitionsCount; ++i) {
        if (strcmp(labelDefinitions[i].name, labelUse->name) == 0) {
            return &labelDefinitions[i];
        }
    }

    printf("Error on line %d: label \"%s\" is undefined.\n", labelUse->lineNumber, labelUse->name);
    exit(ExitCodeUndefinedLabel);
}

static struct Token getNextToken() {
    return getToken(&sourceString, &lineNumber);
}

static struct Token getNextNonEmptyToken() {
    struct Token result = getNextToken();
    if (result.value == NULL) {
        printf("Error on line %d: unexpected end of file.\n", lineNumber);
        exit(ExitCodeUnexpectedEndOfFile);
    }
    return result;
}

static bool isValidLabelDefinitionRemoveColon(struct Token token) {
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
        bool characterValid = ch == '_' || ch >= 'a' && ch <= 'z' || ch >= 'A' && ch <= 'Z' || i > 0 && ch >= '0' && ch <= '9';
        if (!characterValid) {
            printf("Error on line %d: \"%s\" is not a valid label name.\n", token.lineNumber, token.value);
            exit(ExitCodeInvalidLabelName);
        }
    }

    for (int i = 0; i < labelDefinitionsCount; ++i) {
        if (strcmp(labelDefinitions[i].name, token.value) == 0) {
            printf("Error on line %d: label name \"%s\" is not unique.\n", token.lineNumber, token.value);
            exit(ExitCodeLabelNameNotUnique);
        }
    }

    return true;
}

static int parseNumberLiteral(struct Token token) {
    char* endChar;
    int result = strtol(token.value, &endChar, 0);
    if (errno  != 0 || *endChar != 0) {
        printf("Error on line %d: \"%s\" is not a valid number.\n", token.lineNumber, token.value);
        exit(ExitCodeInvalidNumberLiteral);
    }
    return result;
}

static struct LabelUseParseResult parseLabelUse(struct Token token) {
    char* offsetSign = strpbrk(token.value, "+-");
    int offset = 0;
    if (offsetSign != NULL) {
        offset = parseNumberLiteral((struct Token) { token.lineNumber, 0, offsetSign });
        *offsetSign = 0;
    }
    return (struct LabelUseParseResult) { token.value, offset };
}

static bool isHexDigit(char character) {
    return character >= '0' && character <= '9' || character >= 'a' && character <= 'z' || character >= 'A' && character <= 'Z';
}

static struct EscapeSequenceParseResult parseEscapeSequence(struct Token token) {
    switch (token.value[1]) {
        case 'n': 
        case 'N':
            return (struct EscapeSequenceParseResult) { '\n', 2 };
        case 't': 
        case 'T': 
            return (struct EscapeSequenceParseResult) { '\t', 2 };
        case 'r': 
        case 'R': 
            return (struct EscapeSequenceParseResult) { '\r', 2 };
        case '\'':
            return (struct EscapeSequenceParseResult) { '\'', 2 };
        case '"':
            return (struct EscapeSequenceParseResult) { '"', 2 };
        case '\\':
            return (struct EscapeSequenceParseResult) { '\\', 2 };
        case 'x':
        case 'X':
            if (!isHexDigit(token.value[2]) || !isHexDigit(token.value[3])) {
                printf("Error on line %d: invalid escape sequence starting with \"\\%c\".\n", token.lineNumber, token.value[1]);
                exit(ExitCodeInvalidEscapeSequence);
            }
            char numberString[5] = "0x00";
            numberString[2] = token.value[2];
            numberString[3] = token.value[3];
            unsigned char number = parseNumberLiteral((struct Token) { token.lineNumber, 5, numberString });
            return (struct EscapeSequenceParseResult) { number, 4 };
        default:
            printf("Error on line %d: invalid escape sequence \"\\%c\".\n", token.lineNumber, token.value[1]);
            exit(ExitCodeInvalidEscapeSequence);
    }
}

static int parseCharacterLiteral(struct Token token) {
    char* fullTokenValue = token.value;

    bool isNegative = token.value[0] == '-';

    if (isNegative) {
        ++token.value;
    }

    int charLength = 1;
    int character = (isNegative ? -1 : 1) * token.value[1];
    if (token.value[1] == '\\') {
        struct EscapeSequenceParseResult parsed = parseEscapeSequence((struct Token) { token.lineNumber, 0, token.value + 1 });
        character = (isNegative ? -1 : 1) * parsed.character;
        charLength = parsed.length;
    }

    if (token.value[0] != '\''
        || token.value[charLength + 1] != '\''
        || !(token.value[charLength + 2] == '+'
            || token.value[charLength + 2] == '-'
            || token.value[charLength + 2] == 0)) {
        printf("Error on line %d: \"%s\" is not a valid character literal.\n", token.lineNumber, fullTokenValue);
        exit(ExitCodeInvalidCharacterLiteral);
    }

    if (token.value[charLength + 2] == 0) {
        return character;
    }

    int offset = parseNumberLiteral((struct Token) { token.lineNumber, 0, token.value + charLength + 2 });
    return character + offset;
 }

static void insertInstruction(enum Instruction instruction) {
    assertNoMemoryViolation(currentAddress, lineNumber);
    assertNoMemoryViolation(currentAddress + 1, lineNumber);
    result.dataType[currentAddress] = DataTypeInstruction;

    unsigned short instructionCode = instruction << 13;
    struct Token param = getNextNonEmptyToken();

    if (isNumberLiteral(param.value)) {
        int paramValue = parseNumberLiteral(param);
        if (paramValue < 0 || paramValue >= ADDRESS_SPACE_SIZE) {
            printf("Error on line %d: attempting to reference invalid address 0x%04X.\n", param.lineNumber, paramValue);
            exit(ExitCodeReferenceToInvalidAddress);
        } 
        instructionCode |= paramValue;
    } else if (!isImmediateValue(param.value)) {
        if (labelUsesCount == MAX_LABEL_USES - 2) {
            printf("Error on line %d: too many label uses.\n", param.lineNumber);
            exit(ExitCodeTooManyLabelUses);
        }
        struct LabelUseParseResult labelUse = parseLabelUse(param);
        labelUses[labelUsesCount++] =
            (struct LabelUse) { labelUse.name, labelUse.offset, 0, param.lineNumber, currentAddress };
        labelUses[labelUsesCount++] =
            (struct LabelUse) { labelUse.name, labelUse.offset, 1, param.lineNumber, currentAddress + 1 };
    } else {
        if (immediateValueUsesCount == MAX_IMMEDIATE_VAL_USES - 1) {
            printf("Error on line %d: too many immediate value uses.\n", param.lineNumber);
            exit(ExitCodeTooManyImmediateValueUses);
        }
        if (!instructionAcceptsImmediateValue(instruction)) {
            printf("Error on line %d: instruction \"%s\" does not accept an immediate value as an argument.\n", param.lineNumber, getInstructionName(instruction));
            exit(ExitCodeInvalidInstructionArgument);
        }
        immediateValueUses[immediateValueUsesCount++] = (struct ImmediateValueUse) { param.value + 1, param.lineNumber, currentAddress };
    }
    result.programMemory[currentAddress++] = instructionCode;
    result.programMemory[currentAddress++] = instructionCode >> 8;
}

static void updateCurrentAddress(int newAddress, int lineNumber, int labelDefinitionsStartIndex) {
    if (newAddress < 0 || newAddress >= ADDRESS_SPACE_SIZE) {
        printf("Error on line %d: attempting to set origin to an invalid address 0x%04X.\n", lineNumber, newAddress);
        exit(ExitCodeOriginOutOfMemoryRange);
    }
    currentAddress = newAddress;
    for (int i = labelDefinitionsStartIndex; i < labelDefinitionsCount; ++i) {
        labelDefinitions[i].address = newAddress;
    }
}

static void applyOrgDirective(int labelDefinitionsStartIndex) {
    struct Token param = getNextNonEmptyToken();
    int paramValue = parseNumberLiteral(param);
    updateCurrentAddress(paramValue, param.lineNumber, labelDefinitionsStartIndex);
}

static void applyAlignDirective(int labelDefinitionsStartIndex) {
    struct Token param = getNextNonEmptyToken();
    int paramValue = parseNumberLiteral(param);
    if (paramValue < 1 || paramValue > 12) {
        printf("Error on line %d: invalid align argument \"%d\". Must be between 1 and 12.\n", param.lineNumber, paramValue);
        exit(ExitCodeInvalidDirectiveArgument);
    }
    unsigned short bitsToReset = (1 << paramValue) - 1;
    int newAddress = (currentAddress & bitsToReset) == 0
        ? currentAddress
        : ((currentAddress & ~bitsToReset) + bitsToReset + 1);
    updateCurrentAddress(newAddress, param.lineNumber, labelDefinitionsStartIndex);
}

static void applyFillDirective() {
    struct Token valueParam = getNextNonEmptyToken();
    struct Token countParam = getNextNonEmptyToken();

    int value, count;
    enum DataType valueToFillType;
    
    if (isCharacterLiteral(valueParam.value)) {
        valueToFillType = DataTypeChar;
        value = parseCharacterLiteral(valueParam);
        if (value < CHAR_MIN || value > UCHAR_MAX) {
            printf("Error on line %d: character literal \"%s\" evaluates to %d, which is out of range.\n", valueParam.lineNumber, valueParam.value, value);
            exit(ExitCodeCharacterLiteralOutOutRange);
        }
    } else if (isNumberLiteral(valueParam.value)) {
        valueToFillType = DataTypeInt;
        value = parseNumberLiteral(valueParam);
        if (value < CHAR_MIN || value > UCHAR_MAX) {
            printf("Error on line %d: number %d is out of range.\n", valueParam.lineNumber, value);
            exit(ExitCodeNumberLiteralOutOutRange);
        }
    } else {
        printf("Error on line %d: \"%s\" is neither a character nor a number.\n", valueParam.lineNumber, valueParam.value);
        exit(ExitCodeInvalidDirectiveArgument);
    }

    count = parseNumberLiteral(countParam);
    if (count < 1) {
        printf("Error on line %d: fill count must be positive.\n", countParam.lineNumber);
        exit(ExitCodeInvalidDirectiveArgument);
    }

    for (int i = 0; i < count; ++i) {
        assertNoMemoryViolation(currentAddress, countParam.lineNumber);
        result.dataType[currentAddress] = valueToFillType;
        result.programMemory[currentAddress++] = value;
    }
}

static void applyLsbOrMsbDirective(enum Directive directive) {
    struct Token param = getNextNonEmptyToken();
    struct LabelUseParseResult labelUse = parseLabelUse(param);
    int byte = directive == DirectiveLsb ? 0 : 1;
    assertNoMemoryViolation(currentAddress, param.lineNumber);
    result.dataType[currentAddress] = DataTypeInt;
    if (labelUsesCount == MAX_LABEL_USES - 1) {
        printf("Error on line %d: too many label uses.\n", lineNumber);
        exit(ExitCodeTooManyLabelUses);
    }
    labelUses[labelUsesCount++] =
        (struct LabelUse) { labelUse.name, labelUse.offset, byte, param.lineNumber, currentAddress++ };
}

static void applyDirective(enum Directive directive, int labelDefinitionsStartIndex) {
    switch (directive) {
        case DirectiveOrg: return applyOrgDirective(labelDefinitionsStartIndex);
        case DirectiveAlign: return applyAlignDirective(labelDefinitionsStartIndex);
        case DirectiveFill: return applyFillDirective();
        case DirectiveLsb:
        case DirectiveMsb: return applyLsbOrMsbDirective(directive);
        case DirectiveInvalid: break;
    }
}

static void declareString(struct Token token) {
    for (int i = 1; i < token.length - 1; ++i) {
        assertNoMemoryViolation(currentAddress, token.lineNumber);
        result.dataType[currentAddress] = DataTypeChar;
        if (token.value[i] == '\\') {
            struct EscapeSequenceParseResult parsed = parseEscapeSequence((struct Token) { token.length, 0, token.value + i });
            result.programMemory[currentAddress++] = parsed.character;
            i += parsed.length - 1;
        } else {
            result.programMemory[currentAddress++] = token.value[i];
        }
    }
    assertNoMemoryViolation(currentAddress, token.lineNumber);
    result.dataType[currentAddress] = DataTypeChar;
    result.programMemory[currentAddress++] = 0;
}

static void declareNumber(struct Token token) {
    assertNoMemoryViolation(currentAddress, lineNumber);
    result.dataType[currentAddress] = DataTypeInt;
    int number = parseNumberLiteral(token);
    if (number < CHAR_MIN || number > UCHAR_MAX) {
        printf("Error on line %d: number %d is out of range.\n", token.lineNumber, number);
        exit(ExitCodeNumberLiteralOutOutRange);
    }
    result.programMemory[currentAddress++] = number;
}

static void declareCharacter(struct Token token) {
    assertNoMemoryViolation(currentAddress, lineNumber);
    result.dataType[currentAddress] = DataTypeChar;
    int number = parseCharacterLiteral(token);
    if (number < CHAR_MIN || number > UCHAR_MAX) {
        printf("Error on line %d: character literal \"%s\" evaluates to %d, which is out of range.\n", token.lineNumber, token.value, number);
        exit(ExitCodeCharacterLiteralOutOutRange);
    }
    result.programMemory[currentAddress++] = number;
}

static struct Token parseLabelDefinitionsGetNextToken() {
    struct Token token;

    while (true) {
        token = getNextToken();
        if (token.value != NULL && isValidLabelDefinitionRemoveColon(token)) {
            if (labelDefinitionsCount == MAX_LABEL_DEFS - 1) {
                printf("Error on line %d: too many label definitions.\n", token.lineNumber);
                exit(ExitCodeTooManyLabelDefinitions);
            }
            labelDefinitions[labelDefinitionsCount++] = (struct LabelDefinition) { token.value, token.lineNumber, currentAddress };
        } else {
            break;
        }
    }

    return token;
}

/// Returns true if statement parsing should continue
static bool parseStatement() {
    int labelDefinitionsStartIndex = labelDefinitionsCount;
    struct Token firstTokenAfterLabels = parseLabelDefinitionsGetNextToken();

    if (firstTokenAfterLabels.value == NULL) {
        if (labelDefinitionsCount > labelDefinitionsStartIndex) {
            printf("Error on line %d: unexpected label definition at the end of the file.\n", lineNumber);
            exit(ExitCodeUnexpectedEndOfFile);
        }

        return false;
    }

    enum Instruction instruction;
    enum Directive directive;

    if ((instruction = getInstruction(firstTokenAfterLabels.value)) != InstructionInvalid) {
        insertInstruction(instruction);
    } else if ((directive = getDirective(firstTokenAfterLabels.value)) != DirectiveInvalid) {
        applyDirective(directive, labelDefinitionsStartIndex);
    } else if (isStringLiteral(firstTokenAfterLabels.value)) {
        declareString(firstTokenAfterLabels);
    } else if (isNumberLiteral(firstTokenAfterLabels.value)) {
        declareNumber(firstTokenAfterLabels);
    } else if (isCharacterLiteral(firstTokenAfterLabels.value)) {
        declareCharacter(firstTokenAfterLabels);
    } else {
        printf("Error on line %d: invalid token \"%s\".\n", firstTokenAfterLabels.lineNumber, firstTokenAfterLabels.value);
        exit(ExitCodeInvalidToken);
    }

    return true;
}

static void resolveLabels() {
    for (int i = labelDefinitionsCount - 1; i >= 0; --i) {
        result.labelNameByAddress[labelDefinitions[i].address] = labelDefinitions[i].name;
    }

    for (int i = 0; i < labelUsesCount; ++i) {
        struct LabelUse* labelUse = &labelUses[i];
        struct LabelDefinition* labelDefinition = findLabelDefinition(labelUse);
        int evaluatedAddress = labelDefinition->address + labelUse->offset;
        
        if (evaluatedAddress < 0 || evaluatedAddress >= ADDRESS_SPACE_SIZE) {
            printf("Error on line %d: \"%s%s%d\" evaluates to %d, which is an invalid address.\n", labelUse->lineNumber, labelUse->name, labelUse->offset < 0 ? "" : "+", labelUse->offset, evaluatedAddress);
            exit(ExitCodeReferenceToInvalidAddress);
        }
        
        result.programMemory[labelUse->address] |= evaluatedAddress >> (labelUse->byte * 8);
    }
}

struct AssemblerResult assemble(char* source) {
    sourceString = source;

    while (parseStatement()) {}

    resolveLabels();

    return result;
}