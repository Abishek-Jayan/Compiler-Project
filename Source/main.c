#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "lexer.h"
#include "parser.h"
#include "parser_ast.h"
#include "codegen.h"

void show_usage()
{
    fprintf(stderr, "Usage: mycc -mode infile\nValid modes:\n");
    fprintf(stderr, " -0: Version information only\n");
    fprintf(stderr, " -1: Phase 1 Lexer Parsing \n");
    fprintf(stderr, " -2: Phase 2 Parser Parsing \n");
    fprintf(stderr, " -3: Phase 3 Type Checking\n");
    fprintf(stderr, " -4: Phase 4 Code Generation\n");
}

void show_version()
{
    printf("My own C compiler for COMS 5400, Spring\n");
    printf("Written by Abishek Jayan (abishekj@iastate.edu)\n");
    printf("Version 1.0, released 29 January 2025\n");
}


int main(int argc, char *argv[])
{
    if (argc == 1)
    {
        show_usage();
    }

    else if (strcmp(argv[1], "-0") == 0)
    {
        if (argc < 3)
        {
            fprintf(stderr, "Warning: No input file provided for -0 mode. \n");
        }
        show_version();
    }
    else if (strcmp(argv[1], "-1") == 0)
    {
        if (argc < 3)
        {
            fprintf(stderr, "Usage: %s <input file>\n", argv[0]);
            exit(1);
        }
        char *infilename = argv[2];
        FILE *input = fopen(argv[2], "r");
        if (!input)
        {
            printf("Error: No such input file");
            exit(1);
        }
        char *truncated_infilename = malloc(strlen(infilename) - 1);
        if (!truncated_infilename)
        {
            fprintf(stderr, "Failed to allocate memory for output file name");
            return 1;
        }
        strncpy(truncated_infilename, infilename, strlen(infilename) - 2);
        char *outfilename = malloc(strlen(truncated_infilename) + strlen(".lexer") + 1);
        snprintf(outfilename, strlen(truncated_infilename) + strlen(".lexer") + 1, "%s.lexer", truncated_infilename);

        FILE *output = fopen(outfilename, "w");

        lexer L;
        init_lexer(&L, infilename, outfilename);

        while (L.current.ID != END)
        {
            fprintf(output, "File %s Line %d Token %d Text %s\n", L.filename, L.lineno, L.current.ID, L.current.attrb);
            free(L.current.attrb);
            getNextToken(&L);
        }
        fclose(input);
        fclose(output);
        printf("Completed lexing. Check %s for details\n", outfilename);
    }
    else if (strcmp(argv[1], "-2") == 0)
    {
        if (argc < 3)
        {
            fprintf(stderr, "Usage: %s <input file>\n", argv[0]);
            exit(1);
        }
        char *infilename = argv[2];
        FILE *input = fopen(argv[2], "r");
        if (!input)
        {
            printf("Error: No such input file");
            exit(1);
        }
        fclose(input);
        char *truncated_infilename = malloc(strlen(infilename) - 1);
        if (!truncated_infilename)
        {
            fprintf(stderr, "Failed to allocate memory for output file name");
            return 1;
        }
        strncpy(truncated_infilename, infilename, strlen(infilename) - 2);
        char *outfilename = malloc(strlen(truncated_infilename) + strlen(".parser") + 1);
        snprintf(outfilename, strlen(truncated_infilename) + strlen(".parser") + 1, "%s.parser", truncated_infilename);

        lexer L;
        init_lexer(&L, infilename, outfilename);
        FILE *output = fopen(outfilename, "w");

        parser P;
        init_parser(&P, &L, output, infilename, outfilename);
        fclose(L.infile);
        fclose(output);
        printf("Completed parsing. Check %s for details\n", outfilename);
        free(outfilename);
    }
    else if (strcmp(argv[1], "-3") == 0)
    {
        if (argc < 3)
        {
            fprintf(stderr, "Usage: %s <input file>\n", argv[0]);
            exit(1);
        }
        char *infilename = argv[2];
        FILE *input = fopen(argv[2], "r");
        if (!input)
        {
            printf("Error: No such input file");
            exit(1);
        }
        fclose(input);
        char *truncated_infilename = malloc(strlen(infilename) - 1);
        if (!truncated_infilename)
        {
            fprintf(stderr, "Failed to allocate memory for output file name");
            exit(1);
        }
        strncpy(truncated_infilename, infilename, strlen(infilename) - 2);
        char *outfilename = malloc(strlen(truncated_infilename) + strlen(".types") + 1);
        snprintf(outfilename, strlen(truncated_infilename) + strlen(".types") + 1, "%s.types", truncated_infilename);

        lexer L;
        init_lexer(&L, infilename, outfilename);
        FILE *output = fopen(outfilename, "w");

    
        
        // Initialize symbol tables
        
        // Parse the program 
        int stmtCount = 0;
        Statement **program = parse_program(&L, &stmtCount);
        
        for (int i = 0; i < stmtCount; i++) {

            type_check_statement(program[i], infilename,output);
        }
        fclose(output);
        printf("Completed type checking. Check %s for details\n", outfilename);
        free(outfilename);
    }

    else if (strcmp(argv[1], "-4") == 0)
    {
        if (argc < 3)
        {
            fprintf(stderr, "Usage: %s <input file>\n", argv[0]);
            exit(1);
        }
        char *infilename = argv[2];
        FILE *input = fopen(argv[2], "r");
        if (!input)
        {
            printf("Error: No such input file");
            exit(1);
        }
        fclose(input);
        char *truncated_infilename = malloc(strlen(infilename) - 1);
        if (!truncated_infilename)
        {
            fprintf(stderr, "Failed to allocate memory for output file name");
            exit(1);
        }
        strncpy(truncated_infilename, infilename, strlen(infilename) - 2);
        char *outfilename = malloc(strlen(truncated_infilename) + strlen(".j") + 1);
        snprintf(outfilename, strlen(truncated_infilename) + strlen(".j") + 1, "%s.j", truncated_infilename);

        lexer L;
        init_lexer(&L, infilename, outfilename);
        int stmtCount = 0;
        Statement **program = parse_program(&L, &stmtCount);
        if (!program) {
            fprintf(stderr, "Parsing failed for %s\n", infilename);
            exit(1);
        } else {
            
            generate_code(program, stmtCount, infilename, outfilename);
            printf("Completed code generation. Check %s for details\n", outfilename);
        }
        free(outfilename);
    }


    else
    {
        show_usage();
    }
    return 0;
}