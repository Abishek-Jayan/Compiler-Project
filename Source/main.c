#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "lexer.h"


void show_usage() {
    fprintf(stderr, "Usage: mycc -mode infile\nValid modes:\n");
    fprintf(stderr, " -0: Version information only\n");
    fprintf(stderr, " -1: Phase 1 (not yet implemented)\n");
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
            return 1;
        }
        printf("[%u]: Test\n", __LINE__);
        lexer(input, filename, output);
        printf("[%u]: Test\n", __LINE__);
    
        fclose(input);
        fclose(output);
    
        return 0;

    }
    return 0;
    
}