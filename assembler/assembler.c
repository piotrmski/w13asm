#include "assembler.h"
#include "../tokenizer/tokenizer.h"
#include <stdbool.h>
#include <stdio.h>

#define ADDRESS_SPACE_SIZE 0x2000
#define PROGRAM_MEMORY_SIZE 0x1fff
#define MAX_LABELS 0x800

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
    DirectiveFill,
    DirectiveInvalid
};

struct LabelInfo {
    char* name;
    int address;
};

struct AssemblerState {
    int address;
    unsigned short programMemory[PROGRAM_MEMORY_SIZE];
    bool programMemoryWritten[PROGRAM_MEMORY_SIZE];
    bool lastTokenWasLabelDefinition;
    struct LabelInfo labelDefinitions[MAX_LABELS];
    int labelDefinitionsIndex;
    struct LabelInfo labelUses[MAX_LABELS];
    int labelUsesIndex;
};

bool parseToken(struct Token* token, struct AssemblerState* state) {
    // TODO always validate memory write in range and not overwritten
    switch (token->type) {
        case TokenTypeNone:
            return true;
        case TokenTypeLabelDefinition:
            state->labelDefinitions[state->labelDefinitionsIndex].name = token->stringValue;
            state->labelDefinitions[state->labelDefinitionsIndex++].address = state->address;
            state->lastTokenWasLabelDefinition = true;
            break;
        case TokenTypeLabelUseOrInstruction:
            enum Instruction instruction = getInstruction(token->stringValue);
            if (instruction == InstructionInvalid) {
                // error
                return true;
            } else {
                // instruction; get next token; should be label use or number within memory space
            }
            state->lastTokenWasLabelDefinition = false;
            break;
        case TokenTypeDirective:
            enum Directive directive = getDirective(token->stringValue);
            switch (directive) {
                case DirectiveOrg:
                    // get next token; should be number within memory space; update last label
                    break;
                case DirectiveFill:
                    // get 2 next tokens; first should be any number or NZTString length=1; second should be positive number
                    break;
                default:
                    // error
                    return true;
            }
            state->lastTokenWasLabelDefinition = false;
            break;
        case TokenTypeDecimalNumber:
        case TokenTypeHexNumber:
        case TokenTypeOctalNumber:
        case TokenTypeBinaryNumber:
            // put in memory
            state->lastTokenWasLabelDefinition = false;
            break;
        case TokenTypeZTString:
        case TokenTypeNZTString:
            // put in memory
            state->lastTokenWasLabelDefinition = false;
            break;
        default:
            printf("Error on line %d: invalid token \"%s\".", token->lineNumber, token->stringValue);
            return true;
    }

    return false;
}

void assemble(FILE* filePtr) {
    struct Token token = getInitialTokenState();

    struct AssemblerState state = { 0, { 0 }, { false }, false, { { NULL, 0 } }, 0, { { NULL, 0 } }, 0 };

    do {
        getToken(&token, filePtr);
        if (parseToken(&token, &state)) { return; }
    } while (token.type != TokenTypeNone && token.type != TokenTypeError);

    // todo if label at the end then error
}