#include "tokenizer.h"
#include "stdbool.h"
#include "ctype.h"
#include "stdlib.h"
#include "../../common/exit-code.h"

static void zeroTerminate(char** string, int* lineNumber) {
    if (**string == ';' && *(*string + 1) != '\n' && *(*string + 1) != 0) { // If a non-empty comment follows the token
        *(*string + 1) = ';'; // Move the start of the comment one character forward
    }

    if (**string == '\n') {
        ++*lineNumber;
    }

    **string = 0;
    ++*string;
}

struct Token getToken(char** string, int* lineNumber) {
    bool isComment = false;

    // Skip over non-token characters before the token
    while (isspace(**string) || isComment || **string == ';') {
        if (**string == ';') {
            isComment = true;
        } else if (**string == '\n') {
            isComment = false;
            ++*lineNumber;
        }

        ++*string;
    }

    if (**string == 0) {
        return (struct Token) { *lineNumber, NULL };
    }

    int tokenStartLineNumber = *lineNumber;

    char* result = *string;

    // TODO handle ' ' as one token

    if (*result == '"') {
        do { // Find the end of the string
            if (**string == '\n') {
                ++*lineNumber;
            }

            if (**string == '\\' && *(*string + 1) == '"') {
                *string += 2;
            } else {
                ++*string;
            }
        } while (**string != '"' && **string != 0);

        if (**string == 0) {
            printf("Error on line %d: unterminated string literal.\n", *lineNumber);
            exit(ExitCodeUnterminatedString);
        }

        ++*string; // Now **string should be the next character after the closing quote

        if (**string != 0) {
            if (!isspace(**string) && **string != ';') {
                printf("Error on line %d: unexpected character '%c'.\n", *lineNumber, *(*string + 1));
                exit(ExitCodeUnexpectedCharacter);
            }

            zeroTerminate(string, lineNumber);
        }
    } else {
        while (!isspace(**string) && **string != ';' && **string != 0) { // Find the end of the token
            ++*string;
        }

        if (**string != 0) {
            zeroTerminate(string, lineNumber);
        }
    }

    return (struct Token) { tokenStartLineNumber, result };
}
