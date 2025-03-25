# COMS 4400/5400 Compiler Project

## How to Build and Run the Compiler

1. Clone the repository:
   ```git clone git@git.las.iastate.edu:abishekj/compilers.git```
2. cd into ```Compilers/Source``` folder
3. Run ```make```
4. Run ```./mycc``` with appropriate arguments
5. (Optional) Run ```make clean``` to remove the mycc executable and any temporary files made.

## Phase 1
Lexer has been implemented. Run ```./mycc -1 input_filename output_filename``` to run the lexer.

## Phase 2
Parser has been implemented. Run ```./mycc -2 input_filename``` to run the lexer.

## Source Files
1. main.c: Contains the main logic for the compiler. Handles command-line
arguments and displays version information.
2. Makefile: Contains the logic for building the executable.
3. lexer.c: Contains the logic for the lexer (Phase 1)
4. lexer.h: Header file for importing lexer function in main.c
5. parser.c: Contains the logic for the parser (Phase 2)
6. parser.h: Header file for importing parser function in main.c
7. lexer.o ,main.o and parser.o: Files created by makefile for building mycc. Not git tracked so can be ignored.



## High-Level Overview
The compiler is currently in Phase 2. By providing an input file  we can have the parser parse the instructions and get the appropriate output into an output file.