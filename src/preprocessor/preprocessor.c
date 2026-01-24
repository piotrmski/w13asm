#include "preprocessor.h"
#include "../tokenizer/tokenizer.h"
#include "../shared/shared.h"
#include "../../common/exit-code.h"
#include <stdlib.h>
#include <string.h>

#define RESULT_SIZE_INCREMENT 0x1000
#define MACRO_BODY_SIZE_INCREMENT 0x100
#define MACRO_LABELS_SIZE_INCREMENT 0x10
#define MAX_MACROS 0x100
#define MAX_MACRO_PARAMS 0x10

struct Macro {
    char* name;
    char* params[MAX_MACRO_PARAMS];
    int paramsCount;
    struct Token* tokens;
    int tokensCount;
    char** labels;
    int labelsCount;
    int invocationCount;
};

static char* sourceString;
static struct Token* result;
static int tokensCount = 0;
static char* labels[MAX_LABEL_DEFS];
static int labelsCount = 0;
static struct Macro macros[MAX_MACROS];
static int incompleteMacrosCount = 0;
static int completeMacrosCount = 0;

static void assertUniqueAmongGlobalLabelNames(struct Token token) {
    for (int i = 0; i < labelsCount; ++i) {
        if (strcmp(token.value, labels[i]) == 0) {
            printf("Error on line %d: \"%s\" was already defined as a label name.\n", token.lineNumber, token.value);
            exit(ExitCodeNameCollision);
        }
    }
}

static void assertUniqueAmongMacroNames(struct Token token) {
    for (int i = 0; i < incompleteMacrosCount; ++i) {
        if (strcmp(token.value, macros[i].name) == 0) {
            printf("Error on line %d: \"%s\" was already defined as a macro name.\n", token.lineNumber, token.value);
            exit(ExitCodeNameCollision);
        }
    }
}

static void assertUniqueAmongMacroLabelNames(struct Token token, int macroIndex) {
    for (int i = 0; i < macros[macroIndex].labelsCount; ++i) {
        if (strcmp(token.value, macros[macroIndex].labels[i]) == 0) {
            printf("Error on line %d: \"%s\" was already defined as a label name.\n", token.lineNumber, token.value);
            exit(ExitCodeNameCollision);
        }
    }
}

static void assertUniqueAmongAllMacroLabelNames(struct Token token) {
    for (int macroIndex = 0; macroIndex < incompleteMacrosCount; ++macroIndex) {
        assertUniqueAmongMacroLabelNames(token, macroIndex);
    }
}

static void assertUniqueAmongMacroParamNames(struct Token token, int macroIndex) {
    for (int i = 0; i < macros[macroIndex].paramsCount; ++i) {
        if (strcmp(token.value, macros[macroIndex].params[i]) == 0) {
            printf("Error on line %d: \"%s\" was already defined as a parameter name.\n", token.lineNumber, token.value);
            exit(ExitCodeNameCollision);
        }
    }
}

static void assertUniqueAmongAllMacroParamNames(struct Token token) {
    for (int macroIndex = 0; macroIndex < incompleteMacrosCount; ++macroIndex) {
        assertUniqueAmongMacroParamNames(token, macroIndex);
    }
}

static void assertUniqueAmongInstructionNames(struct Token token, const char* role) {
    for (int i = 0; i < 8; ++i) {
        if (stringsEqualCaseInsensitive(token.value, getInstructionName(i))) {
            printf("Error on line %d: \"%s\" is an instruction name and a %s can't share this name.\n", token.lineNumber, token.value, role);
            exit(ExitCodeNameCollision);
        }
    }
}

static struct Token getNextNonEmptyToken() {
    struct Token result = getToken(&sourceString);
    assertTokenNotEmpty(result);
    return result;
}

static void pushToken(struct Token token) {
    if ((tokensCount % RESULT_SIZE_INCREMENT) == RESULT_SIZE_INCREMENT - 1) {
        result = realloc(result, sizeof (struct Token) * (tokensCount + 1 + RESULT_SIZE_INCREMENT));
    }
    result[tokensCount++] = token;
}

static void pushMacroToken(int macroIndex, struct Token token) {
    int cnt = macros[macroIndex].tokensCount;
    if ((cnt % MACRO_BODY_SIZE_INCREMENT) == MACRO_BODY_SIZE_INCREMENT - 1) {
        macros[macroIndex].tokens = realloc(macros[macroIndex].tokens, sizeof (struct Token) * (cnt + 1 + MACRO_BODY_SIZE_INCREMENT));
    }
    macros[macroIndex].tokens[macros[macroIndex].tokensCount++] = token;
}

static void registerMacroLabel(int macroIndex, struct Token token) {
    struct Token labelDefinitionToken = (struct Token) { malloc(token.length - 1), token.length - 1, token.lineNumber, NULL, 0 };
    memcpy(labelDefinitionToken.value, token.value, token.length - 1);
    labelDefinitionToken.value[token.length - 1] = 0;

    assertUniqueAmongGlobalLabelNames(labelDefinitionToken);
    assertUniqueAmongMacroNames(labelDefinitionToken);
    assertUniqueAmongMacroParamNames(labelDefinitionToken, macroIndex);
    assertUniqueAmongMacroLabelNames(labelDefinitionToken, macroIndex);

    int cnt = macros[macroIndex].labelsCount;
    if ((cnt % MACRO_LABELS_SIZE_INCREMENT) == MACRO_LABELS_SIZE_INCREMENT - 1) {
        macros[macroIndex].labels = realloc(macros[macroIndex].labels, sizeof (char*) * (cnt + 1 + MACRO_LABELS_SIZE_INCREMENT));
    }
    macros[macroIndex].labels[macros[macroIndex].labelsCount++] = labelDefinitionToken.value;
}

static void assertNameValid(struct Token token, const char* role) {
    for (int i = 0; i < token.length; ++i) {
        char ch = token.value[i];
        bool characterValid = ch == '_' || ch >= 'a' && ch <= 'z' || ch >= 'A' && ch <= 'Z' || i > 0 && ch >= '0' && ch <= '9';
        if (!characterValid) {
            printf("Error on line %d: \"%s\" is not a valid %s name.\n", token.lineNumber, token.value, role);
            exit(ExitCodeInvalidLabelName);
        }
    }
}

static bool isValidLabelDefinition(struct Token token) {
    if (token.value[token.length - 1] != ':') {
        return false;
    }

    if (token.length > MAX_LABEL_NAME_LEN_INCL_0) {
        printf("Error on line %d: label name too long.\n", token.lineNumber);
        exit(ExitCodeLabelNameTooLong);
    }

    token.value[--token.length] = 0;
    assertNameValid(token, "label");
    token.value[token.length++] = ':';

    return true;
}

static bool isMacroDefinitionStart(struct Token token) {
    return stringsEqualCaseInsensitive(token.value, ".MACRO");
}

static bool isMacroDefinitionEnd(struct Token token) {
    return stringsEqualCaseInsensitive(token.value, ".ENDMACRO");
}

static int getMacroIndexByName(char* name, int macrosCount) {
    for (int i = 0; i < macrosCount; ++i) {
        if (strcmp(name, macros[i].name) == 0) {
            return i;
        }
    }
    return -1;
}

static void registerLabel(struct Token token) {
    struct Token labelDefinitionToken = (struct Token) { malloc(token.length - 1), token.length - 1, token.lineNumber, NULL, 0 };
    memcpy(labelDefinitionToken.value, token.value, token.length - 1);
    labelDefinitionToken.value[token.length - 1] = 0;

    if (labelsCount == MAX_LABEL_DEFS - 1) {
        printf("Error on line %d: too many label definitions.\n", token.lineNumber);
        exit(ExitCodeTooManyLabelDefinitions);
    }

    assertUniqueAmongGlobalLabelNames(labelDefinitionToken);
    assertUniqueAmongMacroNames(labelDefinitionToken);
    assertUniqueAmongAllMacroLabelNames(labelDefinitionToken);
    assertUniqueAmongAllMacroParamNames(labelDefinitionToken);
 
    labels[labelsCount++] = labelDefinitionToken.value;
}

static void getMacroArguments(int macroIndex, char** argumentValues) {
    bool hasNextArg = true;
    int argumentIndex = 0;
    int lineNumber;
    while (hasNextArg) {
        struct Token token = getNextNonEmptyToken();
        lineNumber = token.lineNumber;
        char* argumentValue = token.value;
        if (argumentValue[token.length - 1] == ',') {
            if (argumentIndex == MAX_MACRO_PARAMS) {
                printf("Error on line %d: macro \"%s\" takes %d arguments, over %d were provided.\n", token.lineNumber, macros[macroIndex].name, macros[macroIndex].paramsCount, MAX_MACRO_PARAMS);
                exit(ExitCodeInvalidMacroArgumentsCount);
            }
            argumentValue = malloc(token.length - 1);
            memcpy(argumentValue, token.value, token.length - 1);
            argumentValue[token.length - 1] = 0;
        } else {
            hasNextArg = false;
        }
        argumentValues[argumentIndex++] = argumentValue;
    }

    if (argumentIndex != macros[macroIndex].paramsCount) {
        printf("Error on line %d: macro \"%s\" takes %d arguments, %d were provided.\n", lineNumber, macros[macroIndex].name, macros[macroIndex].paramsCount, argumentIndex);
        exit(ExitCodeInvalidMacroArgumentsCount);
    }
}

static void replaceParamsWithArgs(int macroIndex, struct Token* token, char** argumentValues) {
    for (int i = 0; i < macros[macroIndex].paramsCount; ++i) {
        if (strcmp(macros[macroIndex].params[i], token->value) == 0) {
            token->value = argumentValues[i];
            token->length = strlen(argumentValues[i]);
            return;
        }
    }
}

static void invokeMacro(int macroIndex) {
    char* argumentValues[MAX_MACRO_PARAMS] = {0};

    if (macros[macroIndex].paramsCount > 0) {
        getMacroArguments(macroIndex, argumentValues);
    }

    for (int i = 0; i < macros[macroIndex].tokensCount; ++i) {
        struct Token token = macros[macroIndex].tokens[i];
        token.macroName = macros[macroIndex].name;
        token.macroInvocationIndex = macros[macroIndex].invocationCount;

        replaceParamsWithArgs(macroIndex, &token, argumentValues);

        int nestedMacroIndex = getMacroIndexByName(token.value, macroIndex);
        if (nestedMacroIndex >= 0) {
            invokeMacro(nestedMacroIndex);
        } else {
            pushToken(token);
        }
    }

    ++macros[macroIndex].invocationCount;
}

static void registerMacroParams(int macroIndex) {
    bool hasNextParam = true;
    while (hasNextParam) {
        struct Token token = getNextNonEmptyToken();
        if (token.value[token.length - 1] == ',') {
            if (macros[macroIndex].paramsCount == MAX_MACRO_PARAMS) {
                printf("Error on line %d: too many parameters.\n", token.lineNumber);
                exit(ExitCodeTooManyMacroParams);
            }
            token.value[--token.length] = 0;
        } else {
            hasNextParam = false;
        }

        assertNameValid(token, "parameter");
        assertUniqueAmongGlobalLabelNames(token);
        assertUniqueAmongInstructionNames(token, "parameter");
        assertUniqueAmongMacroNames(token);
        assertUniqueAmongMacroParamNames(token, macroIndex);
        assertUniqueAmongMacroLabelNames(token, macroIndex);

        macros[macroIndex].params[macros[macroIndex].paramsCount++] = token.value;
    }
}

static void registerMacroBody(int macroIndex) {
    while (true) {
        struct Token token = getNextNonEmptyToken();

        if (isValidLabelDefinition(token)) {
            registerMacroLabel(macroIndex, token);
            pushMacroToken(macroIndex, token);
        } else if (isMacroDefinitionStart(token)) {
            printf("Error on line %d: macro definition inside another macro body is illegal.\n", token.lineNumber);
            exit(ExitCodeNestedMacros);
        } else if (isMacroDefinitionEnd(token)) {
            return;
        } else {
            pushMacroToken(macroIndex, token);
        }
    }
}

static void registerMacro() {
    int macroIndex = incompleteMacrosCount++;
    struct Macro* macro = &macros[macroIndex];
    macro->name = "";
    macro->paramsCount = 0;
    macro->tokens = calloc(MACRO_BODY_SIZE_INCREMENT, sizeof (struct Token));
    macro->tokensCount = 0;
    macro->labels = calloc(MACRO_LABELS_SIZE_INCREMENT, sizeof (char *));
    macro->labelsCount = 0;
    macro->invocationCount = 0;

    struct Token name = getNextNonEmptyToken();
    bool hasParams = name.value[name.length - 1] == ',';
    if (hasParams) { name.value[--name.length] = 0; }

    assertNameValid(name, "macro");
    assertUniqueAmongGlobalLabelNames(name);
    assertUniqueAmongInstructionNames(name, "macro");
    assertUniqueAmongMacroNames(name);
    assertUniqueAmongAllMacroParamNames(name);
    assertUniqueAmongAllMacroLabelNames(name);
    
    macro->name = name.value;

    if (hasParams) {
        registerMacroParams(macroIndex);
    }

    registerMacroBody(macroIndex);

    ++completeMacrosCount;
}

static void processTokens() {
    while (true) {
        struct Token token = getToken(&sourceString);

        if (token.value == NULL) {
            pushToken(token);
            return;
        } else if (isValidLabelDefinition(token)) {
            registerLabel(token);
            pushToken(token);
        } else if (isMacroDefinitionStart(token)) {
            registerMacro();
        } else {
            int macroIndex = getMacroIndexByName(token.value, completeMacrosCount);
            if (macroIndex >= 0) {
                invokeMacro(macroIndex);
            } else {
                pushToken(token);
            }
        }
    }
}

struct Token* preprocess(char* assemblySource) {
    sourceString = assemblySource;
    result = calloc(RESULT_SIZE_INCREMENT, sizeof (struct Token));

    processTokens();

    return result;
}