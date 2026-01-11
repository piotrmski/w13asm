#include "preprocessor.h"
#include "../tokenizer/tokenizer.h"
#include "../shared/shared.h"
#include "../../common/exit-code.h"
#include <stdlib.h>
#include <string.h>

#define RESULT_SIZE_INCREMENT 0x1000
#define MACRO_BODY_SIZE_INCREMENT 0x100
#define MAX_MACROS 0x100
#define MAX_MACRO_PARAMS 0x10

struct Macro {
    char* name;
    char* params[MAX_MACRO_PARAMS];
    int paramsCount;
    struct Token* tokens;
    int tokensCount;
};

static char* sourceString;
static struct Token* result;
static int tokensCount = 0;
static char* labels[MAX_LABEL_DEFS];
static int labelsCount = 0;
static struct Macro macros[MAX_MACROS];
static int macrosCount = 0;

static void pushToken(struct Token token) {
    if ((tokensCount % RESULT_SIZE_INCREMENT) == RESULT_SIZE_INCREMENT - 1) {
        result = realloc(result, sizeof (struct Token) * (tokensCount + 1 + RESULT_SIZE_INCREMENT));
    }
    result[tokensCount++] = token;
}

static bool isValidLabelDefinition(struct Token token) {
    if (token.value[token.length - 1] != ':') {
        return false;
    }

    if (token.length > MAX_LABEL_NAME_LEN_INCL_0) {
        printf("Error on line %d: label name too long.\n", token.lineNumber);
        exit(ExitCodeLabelNameTooLong);
    }

    for (int i = 0; i < token.length - 1; ++i) {
        char ch = token.value[i];
        bool characterValid = ch == '_' || ch >= 'a' && ch <= 'z' || ch >= 'A' && ch <= 'Z' || i > 0 && ch >= '0' && ch <= '9';
        if (!characterValid) {
            token.value[token.length - 1] = 0; // Trimming the colon
            printf("Error on line %d: \"%s\" is not a valid label name.\n", token.lineNumber, token.value);
            exit(ExitCodeInvalidLabelName);
        }
    }

    char* labelName = malloc(token.length - 1);
    memcpy(labelName, token.value, token.length - 1);
    labelName[token.length - 1] = 0;

    for (int i = 0; i < macrosCount; ++i) {
        if (strcmp(labelName, macros[i].name) == 0) {
            printf("Error on line %d: \"%s\" was already defined as a macro name.\n", token.lineNumber, labelName);
            exit(ExitCodeNameCollision);
        }
    }
 
    labels[labelsCount++] = labelName;

    return true;
}

static bool isMacroDefinitionStart(struct Token token) {
    return stringsEqualCaseInsensitive(token.value, ".MACRO");
}

static bool isMacroInvocation(struct Token token) {
    // TODO
    return false;
}

static void registerLabel(char* labelNameWithColon) {
    // TODO
}

static void registerMacro() {
    // TODO
}

static void invokeMacro(char* macroName) {
    // TODO
}

static void processToken() {
    struct Token token = getToken(&sourceString);

    if (token.value == NULL) {
        pushToken(token);
    } else if (isValidLabelDefinition(token)) {
        registerLabel(token.value);
        pushToken(token);
    } else if (isMacroDefinitionStart(token)) {
        registerMacro();
    } else if (isMacroInvocation(token)) {
        invokeMacro(token.value);
    } else {
        pushToken(token);
    }
}

struct Token* preprocess(char* assemblySource) {
    sourceString = assemblySource;
    result = calloc(RESULT_SIZE_INCREMENT, sizeof (struct Token));

    do {
        processToken();
    } while (result[tokensCount - 1].value != NULL);

    return result;
}