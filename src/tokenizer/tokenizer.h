#ifndef tokenizer
#define tokenizer

#include <stdbool.h>
#include <stdio.h>

struct Token {
    int lineNumber;
    int length;
    char* value;
};

/**
 * Returned structure field value points to the first token in the string
 * pointed to by `*string`, or NULL if there isn't any token in the string.
 * Replaces the first non-token character of the string with 0.
 * Updates `*string` to point to the first character after the 0.
 */
struct Token getToken(char** string);

#endif