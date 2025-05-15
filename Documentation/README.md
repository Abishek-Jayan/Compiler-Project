# COMS 4400/5400 Compiler Project

## How to Build and Run the Compiler

1. Clone the repository:
   ```git clone git@git.las.iastate.edu:abishekj/compilers.git```
2. cd into ```Compilers/Source``` folder
3. Run ```make```
4. Run ```./mycc``` with appropriate arguments
5. (Optional) Run ```make clean``` to remove the mycc executable and any temporary files made.
6. When adding input and output files, make sure to add them into the Source folder.

## Phase 1
Basic code setup has been implemented.

## Phase 2
Lexer has been implemented. Run ```./mycc -1 input_filename output_filename``` to run the lexer.

## Phase 3
Parser has been implemented. Run ```./mycc -2 input_filename``` to run the parser.

## Phase 4
Type Checker has been implemented. Run ```./mycc -3 input_filename``` to run the type checker.

## Phase 5
Code Generator has been implemented. Run ```./mycc -4 input_filename``` to run the type checker.

## Phase 6
Code Generator - Statements and Control Flow has been implemented. Run ```./mycc -5 input_filename``` to run the type checker.

## Source Files
1. main.c: Contains the main logic for the compiler. Handles command-line
arguments and displays version information.
2. Makefile: Contains the logic for building the executable.
3. lexer.c: Contains the logic for the lexer (Phase 2)
4. lexer.h: Header file for importing lexer function in main.c
5. parser.c: Contains the logic for the parser (Phase 3)
6. parser.h: Header file for importing parser function in main.c
7. lexer.o ,main.o and parser.o: Files created by makefile for building mycc. Not git tracked so can be ignored.
8. parser_ast.c: Contains the logic for the type checker (Phase 4)
9. parser_ast.h: Header file for importing type checker function in main.c
10. codegen.c: Contains the logic for the code generation (Phase 5)
11. codegen.h: Header file for importing code generator in codegen.c


## High-Level Overview
The compiler is currently in Phase 6. By providing an input file  we can have the codegenerator generate the parsed code and get the appropriate output into an output file.