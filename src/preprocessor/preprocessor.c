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

static struct Token getNextNonEmptyToken() {
    struct Token result = getToken(sourceString);
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

static void pushMacroLabel(int macroIndex, struct Token token) {
    char* labelName = malloc(token.length - 1);
    memcpy(labelName, token.value, token.length - 1);
    labelName[token.length - 1] = 0;

    int cnt = macros[macroIndex].labelsCount;
    if ((cnt % MACRO_LABELS_SIZE_INCREMENT) == MACRO_LABELS_SIZE_INCREMENT - 1) {
        macros[macroIndex].labels = realloc(macros[macroIndex].labels, sizeof (char*) * (cnt + 1 + MACRO_LABELS_SIZE_INCREMENT));
    }
    macros[macroIndex].labels[macros[macroIndex].labelsCount++] = labelName;
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

static int getMacroIndexByName(char* name) {
    for (int i = 0; i < completeMacrosCount; ++i) {
        if (strcmp(name, macros[i].name) == 0) {
            return i;
        }
    }
    return -1;
}

static void registerLabel(struct Token token) {
    char* labelName = malloc(token.length - 1);
    memcpy(labelName, token.value, token.length - 1);
    labelName[token.length - 1] = 0;

    for (int i = 0; i < incompleteMacrosCount; ++i) {
        if (strcmp(labelName, macros[i].name) == 0) {
            printf("Error on line %d: \"%s\" was already defined as a macro name.\n", token.lineNumber, labelName);
            exit(ExitCodeNameCollision);
        }
    }

    if (labelsCount == MAX_LABEL_DEFS - 1) {
        printf("Error on line %d: too many label definitions.\n", token.lineNumber);
        exit(ExitCodeTooManyLabelDefinitions);
    }

    for (int i = 0; i < labelsCount; ++i) {
        if (strcmp(labelName, labels[i]) == 0) {
            printf("Error on line %d: label name \"%s\" is not unique.\n", token.lineNumber, token.value);
            exit(ExitCodeNameCollision);
        }
    }
 
    labels[labelsCount++] = labelName;
}

static void registerMacroLabel(int macroIndex, struct Token token) {
    // TODO
}

static void invokeMacro(int macroIndex) {
    for (int i = 0; i < macros[macroIndex].tokensCount; ++i) {
        struct Token* token = &macros[macroIndex].tokens[i];
        pushToken((struct Token) { token->value, token->length, token->lineNumber, macros[macroIndex].name, macros[macroIndex].invocationCount });
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
        // TODO check name collisions
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
    // TODO check collisions
    macro->name = name.value;

    if (hasParams) {
        registerMacroParams(macroIndex);
    }

    registerMacroBody(macroIndex);

    ++completeMacrosCount;
}

static void processToken() {
    struct Token token = getToken(&sourceString);

    if (token.value == NULL) {
        pushToken(token);
    } else if (isValidLabelDefinition(token)) {
        registerLabel(token);
        pushToken(token);
    } else if (isMacroDefinitionStart(token)) {
        registerMacro();
    } else {
        int macroIndex = getMacroIndexByName(token.value);
        if (macroIndex >= 0) {
            invokeMacro(macroIndex);
        } else {
            pushToken(token);
        }
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