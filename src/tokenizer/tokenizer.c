#include "tokenizer.h"
#include "stdbool.h"
#include "ctype.h"
#include "stdlib.h"
#include "../../common/exit-code.h"

static int lineNumber = 1;

static void skipUntilTokenStart(char** string) {
    bool isComment = false;

    while (isspace(**string) || isComment && **string != 0 || **string == ';') {
        if (**string == ';') {
            isComment = true;
        } else if (**string == '\n') {
            isComment = false;
            ++lineNumber;
        }

        ++*string;
    }
}

static void skipUntilAfterStringEnd(char** string) {
    char terminator = **string;

    do {
        if (**string == '\n') {
            ++lineNumber;
        }

        if (**string == '\\' && *(*string + 1) == terminator) {
            *string += 2;
        } else {
            ++*string;
        }
    } while (**string != terminator && **string != 0);

    if (**string == 0) {
        printf("Error on line %d: unterminated %s literal.\n", lineNumber, terminator == '"' ? "string" : "character");
        exit(ExitCodeUnterminatedString);
    }

    ++*string;
}

static void skipUntilAfterTokenEnd(char** string) {
    while (!isspace(**string) && **string != ';' && **string != 0) {
        if (**string == '"' || **string == '\'') {
            skipUntilAfterStringEnd(string);
        } else {
            ++*string;
        }
    }
}

static void zeroTerminate(char** string) {
    if (**string == ';' && *(*string + 1) != '\n' && *(*string + 1) != 0) { // If a non-empty comment follows the token
        *(*string + 1) = ';'; // Move the start of the comment one character forward
    }

    if (**string == '\n') {
        ++lineNumber;
    }

    **string = 0;
    ++*string;
}

struct Token getToken(char** string) {
    skipUntilTokenStart(string);

    if (**string == 0) {
        return (struct Token) { lineNumber, 0, NULL };
    }

    int tokenStartLineNumber = lineNumber;

    char* result = *string;

    skipUntilAfterTokenEnd(string);

    char* end = *string;

    if (**string != 0) {
        zeroTerminate(string);
    }

    return (struct Token) { tokenStartLineNumber, end - result, result };
}
