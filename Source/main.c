#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "lexer.h"


void show_usage() {
    fprintf(stderr, "Usage: mycc -mode infile\nValid modes:\n");
    fprintf(stderr, " -0: Version information only\n");
    fprintf(stderr, " -1: Phase 1 Lexer Parsing \n");
    fpritf(stderr, " -2: Phase 2 Parser Parsing \n");
}

void show_version() {
    printf("My own C compiler for COMS 5400, Spring\n");
    printf("Written by Abishek Jayan (abishekj@iastate.edu)\n");
    printf("Version 1.0, released 29 January 2025\n");
}


int main(int argc, char *argv[]) {
    if (argc == 1) {
        show_usage();
    }

    else if(strcmp(argv[1], "-0") == 0) {
        if (argc < 3) {
            fprintf(stderr, "Warning: No input file provided for -0 mode. \n");
        }
        show_version();
    }
    else if(strcmp(argv[1], "-1") == 0){
        if (argc < 4) {
            fprintf(stderr, "Usage: %s <input file> <output file>\n", argv[0]);
            return 1;
        }
        char *filename = argv[2];
        FILE *input = fopen(argv[2], "r");
        if (!input) {
            perror("Error opening input file");
            return 1;
        }

        FILE *output = fopen(argv[3], "w");
        if (!output) {
            perror("Error opening output file");
            fclose(input);
            return 1;
        }
        lexer(input, filename, output);
    
        fclose(input);
        fclose(output);
        printf("Completed lexing. Check %s for details\n",argv[3]);

    }
    else if(strcmp(argv[1],"-2" == 0)) {
        if (argc < 3) {
            fprintf(stderr, "Usage: %s <input file>\n", argv[0]);
            return 1;
        }
        FILE *input = fopen(argv[3],"r");
        if (!input) {
            perror("Error opening input file");
            return 1;
        }
        parser(input);


    }
    else {
        show_usage();
    }
    return 0;
    
}