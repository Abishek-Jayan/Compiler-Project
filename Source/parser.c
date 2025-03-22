#include <stdio.h>
#include <stdlib.h>
#include <string.h>

void parse_program(FILE *input);
void parse_variable_declaration();
void parse_statement();
void parse_type_name();
void parse_function_declaration();
void parse_formal_parameter();
void parse_function_definition();
void parse_statement();
void parse_statement();
void parse_expression();
void parse_l_value();
void parse_unary_operator();
void parse_binary_operator();
void parse_assignment_operator();
void print_parsed_line(FILE *output, char *filename, int lineNumber , char *kind, char *ident);
void parser_error_declaration(char *filename, int lineNumber, char *lexeme, char *expected);


void parser_error_declaration(char *filename, int lineNumber, char *lexeme, char *expected) {
    fprintf(stderr, "Parser error in file %s line %s at text %s \n Expected %s", filename, lineNumber, lexeme, expected);
}

void print_parsed_line(FILE *output, char *filename, int lineNumber , char *kind, char *ident) {
    fprintf(output,"File %s Line %s: %s %s", filename, lineNumber, kind, ident);
}

void parse_program(FILE *input) {
    char line[1024];
    int lineNumber = 1;
    

    //If successfully parsed input file, create 
    FILE *out = fopen("extension.parser","w");
    if (!out) {
        perror("Could not open file");
        exit(1);
    }
    while (fgets(line,sizeof(line),input)) {
        char firstWord[256];
        if (sscanf(line, '%255s', firstWord)){
        if (strcmp(firstWord, "int") == 0 || strcmp(firstWord, "float") == 0 ||
        strcmp(firstWord, "char") == 0 || strcmp(firstWord, "void") == 0)
        {
            parse_variable_declaration(line);
            parse_function_declaration(line);
            parse_function_definition(line);
        }

    }
    }

};



