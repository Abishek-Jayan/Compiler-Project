#ifndef CODEGEN_H
#define CODEGEN_H


#include "parser_ast.h"

typedef struct CodegenContext {
    FILE *output;
    const char *filename;
    Function *currentfunc;
    int localcount;
    int stacksize;
    int maxstacksize;

} CodegenContext;

void generate_code(Statement **stmts, int stmtCount, const char *filename, const char *outfilename);

#endif