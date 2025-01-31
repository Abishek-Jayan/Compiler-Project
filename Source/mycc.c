#include <stdio.h>
#include <stdlib.h>
#include <string.h>

void show_usage() {
    fprintf(stderr, "Usage: mycc -mode infile\nValid modes:\n");
    fprintf(stderr, " -0: Version information only\n");
    fprintf(stderr, " -1: Phase 1 (not yet implemented)\n");
}

void show_version() {
    printf("My own C compiler for COMS 4400/5400, Spring\n");
    printf("Written by Abishek Jayan (abishekj@iastate.edu)");
    printf("Version 1.0, released 29 January 2025\n");
}


int main(int argc, char *argv[]) {
    if (argc == 1) {
        show_usage();
        return 1;
    }

    else if(strcmp(argv[1], "-0") == 0) {
        show_version();
        return 0;
    }
    else {
        show_usage();
        return 1;
    }
    
}