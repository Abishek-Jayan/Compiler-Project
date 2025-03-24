#include <stdio.h> // For FILE type
#include <stdbool.h>
#ifndef PARSER_H
#define PARSER_H

#include "lexer.h"

typedef struct {
    lexer *L;
    token current_token;
    FILE *output;
    char *filename;
    char *outfilename;
    bool is_inside_function;
} parser;

void init_parser(parser *P, lexer *L, FILE *output, char *infilename, char *outfilename);







#endif