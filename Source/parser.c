#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "parser.h"

void parse(parser *P);
void parse_declaration(parser *P);
void parse_variable_list(parser *P, char *ident, unsigned line, char *kind);
void parse_function_definition(parser *P);
void parse_formal_parameter(parser *P);
void parse_statement(parser *P);
void parse_if_statement(parser *P);

void parse_for_statement(parser *P);
void parse_while_statement(parser *P);
void parse_do_while_statement(parser *P);
void parse_type_specifier(parser *P);
void parse_statement_block(parser *P);
void parse_assignment_expression(parser *P);
void parse_conditional_expression(parser *P);
void parse_logical_or_expression(parser *P);
void parse_logical_and_expression(parser *P);
void parse_bitwise_or_expression(parser *P);
void parse_bitwise_and_expression(parser *P);
void parse_equality_expression(parser *P);
void parse_comparison_expression(parser *P);
void parse_additive_expression(parser *P);
void parse_multiplicative_expression(parser *P);
void parse_unary_expression(parser *P);
void parse_primary_expression(parser *P);
void advance(parser *P);
void match(parser *P, unsigned expected_id);

// Helper function that puts next token in the parser
void advance(parser *P)
{

    if (P->current_token.attrb)
    {
        free(P->current_token.attrb);
    }
    getNextToken(P->L);
    P->current_token = P->L->current;
}

// Helper function to check if current token matches expected token
void match(parser *P, unsigned expected_id)
{
    if (P->current_token.ID == expected_id)
    {
        advance(P);
    }
    else
    {
        fprintf(stderr, "Parser error in file %s at line number %d text %s: Expected token %d but got %d \n", P->filename, P->current_token.lineno, P->current_token.attrb ? P->current_token.attrb : "", expected_id, P->current_token.ID);
        remove(P->outfilename);
        exit(1);
    }
}

// Initialise the Parser object
void init_parser(parser *P, lexer *L, FILE *output, char *infilename, char *outfilename)
{
    P->L = L;
    P->output = output;
    P->filename = infilename;
    P->outfilename = outfilename;
    P->is_inside_function = false;
    P->current_token = L->current;
    parse(P);
}

// Main parse function to be called in main.c
void parse(parser *P)
{
    while (P->current_token.ID != END)
    {

        if (P->current_token.ID == TOKEN_TYPE || P->current_token.ID == TOKEN_STRUCT || P->current_token.ID == TOKEN_CONST)
        {
            parse_declaration(P);
        }
        else
        {
            fprintf(stderr, "Parser error in file %s in line %d at text %s: Expected function or global declaration\n", P->filename, P->current_token.lineno, P->current_token.attrb);
            remove(P->outfilename);
            exit(1);
        }
    }
}

// Checks if current token is a function or a variable, calling the corresponding function for each
void parse_declaration(parser *P)
{
    if (P->current_token.ID == TOKEN_STRUCT)
    {
        advance(P);
        if (P->current_token.ID != TOKEN_IDENTIFIER)
        {
            fprintf(stderr, "Parser error in file %s line %d at text %s: Expected struct name\n",
                    P->filename, P->current_token.lineno, P->current_token.attrb);
            remove(P->outfilename);
            exit(1);
        }
        char *struct_name = strdup(P->current_token.attrb);
        unsigned line = P->current_token.lineno;
        advance(P);
        if (P->current_token.ID == TOKEN_LBRACE)
        {
            // Struct definition
            fprintf(P->output, "File %s Line %d: %s struct %s\n",
                    P->filename, line, P->is_inside_function ? "local" : "global", struct_name);
            match(P, TOKEN_LBRACE);
            while (P->current_token.ID != TOKEN_RBRACE && P->current_token.ID != END)
            {
                parse_type_specifier(P);
                while (true)
                {
                    if (P->current_token.ID != TOKEN_IDENTIFIER)
                    {
                        fprintf(stderr, "Parser error in file %s line %d at text %s: Expected identifier\n",
                                P->filename, P->current_token.lineno, P->current_token.attrb);
                        remove(P->outfilename);
                        exit(1);
                    }
                    char *member_ident = strdup(P->current_token.attrb);
                    unsigned member_line = P->current_token.lineno;
                    advance(P);
                    parse_variable_list(P, member_ident, member_line, "member");
                    free(member_ident);
                    if (P->current_token.ID == TOKEN_COMMA)
                    {
                        advance(P);
                    }
                    else
                    {
                        break;
                    }
                }
                match(P, TOKEN_SEMICOLON);
            }
            match(P, TOKEN_RBRACE);
            match(P, TOKEN_SEMICOLON);
        }
        else if (P->current_token.ID == TOKEN_IDENTIFIER)
        {
            char *ident = strdup(P->current_token.attrb); // e.g., "strange" or "p"
            unsigned ident_line = P->current_token.lineno;
            advance(P);
            if (P->current_token.ID == TOKEN_LPAREN)
            {
                // Function definition or prototype, e.g., "struct point strange(int z)"
                fprintf(P->output, "File %s Line %d: function %s\n", P->filename, ident_line, ident);
                parse_function_definition(P);
            }
            else
            {
                // Variable declaration, e.g., "struct point p;"
                parse_variable_list(P, ident, ident_line, P->is_inside_function ? "local variable" : "global variable");
                if (P->current_token.ID == TOKEN_EQUAL)
                {
                    advance(P);
                    parse_assignment_expression(P);
                }
                while (P->current_token.ID == TOKEN_COMMA)
                {
                    advance(P);
                    if (P->current_token.ID != TOKEN_IDENTIFIER)
                    {
                        fprintf(stderr, "Parser error in file %s line %d at text %s: Expected identifier after comma\n",
                                P->filename, P->current_token.lineno, P->current_token.attrb);
                        remove(P->outfilename);
                        exit(1);
                    }
                    free(ident);
                    ident = strdup(P->current_token.attrb);
                    unsigned new_line = P->current_token.lineno;
                    advance(P);
                    parse_variable_list(P, ident, new_line, P->is_inside_function ? "local variable" : "global variable");
                    if (P->current_token.ID == TOKEN_EQUAL)
                    {
                        advance(P);
                        parse_assignment_expression(P);
                    }
                }
                match(P, TOKEN_SEMICOLON);
            }
            free(ident);
        }
        else
        {
            fprintf(stderr, "Parser error in file %s line %d: Expected '{' or identifier after struct name\n",
                    P->filename, P->current_token.lineno);
            remove(P->outfilename);
            exit(1);
        }
        free(struct_name);
    }
    else
    {
        parse_type_specifier(P);
        if (P->current_token.ID != TOKEN_IDENTIFIER)
        {
            fprintf(stderr, "Parser error in file %s line %d at text %s: Expected identifier\n", P->filename, P->current_token.lineno, P->current_token.attrb);
            remove(P->outfilename);
            exit(1);
        }
        char *ident = strdup(P->current_token.attrb);
        unsigned line = P->current_token.lineno;
        advance(P);
        if (P->current_token.ID == TOKEN_LPAREN)
        {
            if(P->is_inside_function == true) {
                fprintf(stderr, "Parser error in file %s line %d: Cannot nest functions", P->filename,P->current_token.lineno);
                remove(P->outfilename);
                exit(1);
            }
            fprintf(P->output, "File %s Line %d: function %s\n", P->filename, line, ident);
            parse_function_definition(P);
        }
        else
        {
            parse_variable_list(P, ident, line, P->is_inside_function ? "local variable" : "global variable");
            if (P->current_token.ID == TOKEN_EQUAL)
            {
                advance(P);
                parse_assignment_expression(P);
            }
            while (P->current_token.ID == TOKEN_COMMA)
            {
                advance(P);
                if (P->current_token.ID != TOKEN_IDENTIFIER)
                {
                    fprintf(stderr, "Parser error in file %s line %d at text %s: Expected identifier after comma\n",
                        P->filename, P->current_token.lineno, P->current_token.attrb);                    remove(P->outfilename);
                    exit(1);
                }
                free(ident);
                ident = strdup(P->current_token.attrb);
                line = P->current_token.lineno;
                advance(P);
                parse_variable_list(P, ident, line, P->is_inside_function ? "local variable" : "global variable");
                if (P->current_token.ID == TOKEN_EQUAL)
                {
                    advance(P);
                    parse_assignment_expression(P);
                }
            }
            match(P, TOKEN_SEMICOLON);
        }
        free(ident);
    }
}

// Handles const and struct
void parse_type_specifier(parser *P)
{
    bool has_const = false;
    if (P->current_token.ID == TOKEN_CONST)
    {
        has_const = true;
        advance(P);
    }
    if (P->current_token.ID == TOKEN_TYPE)
    {
        advance(P);
    }
    else if (P->current_token.ID == TOKEN_STRUCT)
    {
        advance(P);
        if (P->current_token.ID != TOKEN_IDENTIFIER)
        {
            fprintf(stderr, "Parser error in file %s line %d text %s: Expected struct name\n",
                    P->filename, P->current_token.lineno, P->current_token.attrb);
            remove(P->outfilename);
            exit(1);
        }
        advance(P);
    }
    else
    {
        fprintf(stderr, "Parser error in file %s line %d text %s: Expected character missing\n",
                P->filename, P->current_token.lineno, P->current_token.attrb);
        remove(P->outfilename);
        exit(1);
    }
    if (P->current_token.ID == TOKEN_CONST)
    {
        if (has_const)
        {
            fprintf(stderr, "Parser error in file %s line %d: Duplicate const\n",
                    P->filename, P->current_token.lineno);
            remove(P->outfilename);
            exit(1);
        }
        advance(P);
    }
}

//  Checks if the current token is a variable
void parse_variable_list(parser *P, char *ident, unsigned line, char *kind)
{
    if (P->current_token.ID == TOKEN_LBRACKET)
    {
        advance(P);
        if (P->current_token.ID != TOKEN_INT)
        {
            fprintf(stderr, "Parser error in file %s line %d at text %s: Expected integer literal for array size\n", P->filename, P->current_token.lineno, P->current_token.attrb);
            remove(P->outfilename);
            exit(1);
        }
        advance(P);
        match(P, TOKEN_RBRACKET);
    }
    fprintf(P->output, "File %s Line %d: %s %s\n", P->filename, line, kind, ident);
}

// Checks if current token is a function definition
void parse_function_definition(parser *P)
{
    match(P, TOKEN_LPAREN);
    if (P->current_token.ID != TOKEN_RPAREN)
    {
        while (true)
        {
            parse_formal_parameter(P);
            if (P->current_token.ID == TOKEN_COMMA)
            {
                advance(P);
            }
            else
            {
                break;
            }
        }
    }
    match(P, TOKEN_RPAREN);
    if (P->current_token.ID == TOKEN_SEMICOLON)
    {
        advance(P);
    }
    else
    {
        match(P, TOKEN_LBRACE);
        P->is_inside_function = true;
        while (P->current_token.ID != TOKEN_RBRACE && P->current_token.ID != END)
        {

            if (P->current_token.ID == TOKEN_TYPE || P->current_token.ID == TOKEN_STRUCT || P->current_token.ID == TOKEN_CONST)
            {
                parse_declaration(P); // Local variables
            }
            else
            {
                parse_statement(P);
            }
        }
        match(P, TOKEN_RBRACE);
        P->is_inside_function = false;
    }
}

//
void parse_formal_parameter(parser *P)
{
    parse_type_specifier(P);

    if (P->current_token.ID != TOKEN_IDENTIFIER)
    {
        fprintf(stderr, "Parser error in file %s line %d at text %s: Expected identifier for parameter\n", P->filename, P->current_token.lineno, P->current_token.attrb);
        remove(P->outfilename);
        exit(1);
    }

    char *ident = strdup(P->current_token.attrb);
    unsigned line = P->current_token.lineno;
    advance(P);
    if (P->current_token.ID == TOKEN_LBRACKET)
    {
        advance(P);
        match(P, TOKEN_RBRACKET);
    }
    fprintf(P->output, "File %s Line %d: parameter %s\n", P->filename, line, ident);
    free(ident);
}

void parse_statement(parser *P)
{
    if (P->current_token.ID == TOKEN_SEMICOLON)
    {
        advance(P);
    }
    else if (P->current_token.ID == TOKEN_BREAK)
    {
        advance(P);
        match(P, TOKEN_SEMICOLON);
    }
    else if (P->current_token.ID == TOKEN_CONTINUE)
    {
        advance(P);
        match(P, TOKEN_SEMICOLON);
    }
    else if (P->current_token.ID == TOKEN_RETURN)
    {
        advance(P);
        if (P->current_token.ID != TOKEN_SEMICOLON)
        {
            parse_assignment_expression(P);
        }
        match(P, TOKEN_SEMICOLON);
    }
    else if (P->current_token.ID == TOKEN_IF)
    {
        parse_if_statement(P);
    }
    else if (P->current_token.ID == TOKEN_FOR)
    {
        parse_for_statement(P);
    }
    else if (P->current_token.ID == TOKEN_WHILE)
    {
        parse_while_statement(P);
    }
    else if (P->current_token.ID == TOKEN_DO)
    {
        parse_do_while_statement(P);
    }
    else if (P->current_token.ID == TOKEN_LBRACE)
    {
        parse_statement_block(P);
    }
    else
    {
        parse_assignment_expression(P);
        match(P, TOKEN_SEMICOLON);
    }
}

void parse_if_statement(parser *P)
{
    match(P, TOKEN_IF);
    match(P, TOKEN_LPAREN);
    parse_assignment_expression(P);
    match(P, TOKEN_RPAREN);
    parse_statement(P);
    if (P->current_token.ID == TOKEN_ELSE)
    {
        advance(P);
        parse_statement(P);
    }
}

void parse_for_statement(parser *P)
{
    match(P, TOKEN_FOR);
    match(P, TOKEN_LPAREN);
    if (P->current_token.ID != TOKEN_SEMICOLON)
        parse_assignment_expression(P);
    match(P, TOKEN_SEMICOLON);
    if (P->current_token.ID != TOKEN_SEMICOLON)
        parse_assignment_expression(P);
    match(P, TOKEN_SEMICOLON);
    if (P->current_token.ID != TOKEN_RPAREN)
        parse_assignment_expression(P);
    match(P, TOKEN_RPAREN);
    parse_statement(P);
}

void parse_while_statement(parser *P)
{
    match(P, TOKEN_WHILE);
    match(P, TOKEN_LPAREN);
    parse_assignment_expression(P);
    match(P, TOKEN_RPAREN);
    parse_statement(P);
}

void parse_do_while_statement(parser *P)
{
    match(P, TOKEN_DO);
    parse_statement(P);
    match(P, TOKEN_WHILE);
    match(P, TOKEN_LPAREN);
    parse_assignment_expression(P);
    match(P, TOKEN_RPAREN);
    match(P, TOKEN_SEMICOLON);
}

void parse_statement_block(parser *P)
{
    match(P, TOKEN_LBRACE);
    while (P->current_token.ID != TOKEN_RBRACE && P->current_token.ID != END)
    {
        if (P->current_token.ID == TOKEN_TYPE || P->current_token.ID == TOKEN_STRUCT || P->current_token.ID == TOKEN_CONST)
        {
            parse_declaration(P);
        }
        else
        {
            parse_statement(P);
        }
    }
    match(P, TOKEN_RBRACE);
}

void parse_assignment_expression(parser *P)
{
    parse_conditional_expression(P);
    while (P->current_token.ID == TOKEN_EQUAL || P->current_token.ID == TOKEN_ADD_ASSIGN ||
           P->current_token.ID == TOKEN_SUB_ASSIGN || P->current_token.ID == TOKEN_MUL_ASSIGN ||
           P->current_token.ID == TOKEN_DIV_ASSIGN)
    {
        advance(P);
        parse_assignment_expression(P);
    }
}

void parse_conditional_expression(parser *P)
{
    parse_logical_or_expression(P);
    if (P->current_token.ID == TOKEN_QUESTION)
    {
        advance(P);
        parse_assignment_expression(P);
        match(P, TOKEN_COLON);
        parse_conditional_expression(P);
    }
}

void parse_logical_or_expression(parser *P)
{
    parse_logical_and_expression(P);
    while (P->current_token.ID == TOKEN_OR)
    {
        advance(P);
        parse_logical_and_expression(P);
    }
}

void parse_logical_and_expression(parser *P)
{
    parse_bitwise_or_expression(P);
    while (P->current_token.ID == TOKEN_AND)
    {
        advance(P);
        parse_bitwise_or_expression(P);
    }
}

void parse_bitwise_or_expression(parser *P)
{
    parse_bitwise_and_expression(P);
    while (P->current_token.ID == TOKEN_PIPE)
    {
        advance(P);
        parse_bitwise_and_expression(P);
    }
}

void parse_bitwise_and_expression(parser *P)
{
    parse_equality_expression(P);
    while (P->current_token.ID == TOKEN_AMPERSAND)
    {
        advance(P);
        parse_equality_expression(P);
    }
}

void parse_equality_expression(parser *P)
{
    parse_comparison_expression(P);
    while (P->current_token.ID == TOKEN_EQ || P->current_token.ID == TOKEN_NE)
    {
        advance(P);
        parse_comparison_expression(P);
    }
}

void parse_comparison_expression(parser *P)
{
    parse_additive_expression(P);
    while (P->current_token.ID == TOKEN_LESS || P->current_token.ID == TOKEN_LE ||
           P->current_token.ID == TOKEN_GREATER || P->current_token.ID == TOKEN_GE)
    {
        advance(P);
        parse_additive_expression(P);
    }
}

void parse_additive_expression(parser *P)
{
    parse_multiplicative_expression(P);
    while (P->current_token.ID == TOKEN_PLUS || P->current_token.ID == TOKEN_MINUS)
    {
        advance(P);
        parse_multiplicative_expression(P);
    }
}

void parse_multiplicative_expression(parser *P)
{
    parse_unary_expression(P);
    while (P->current_token.ID == TOKEN_ASTERISK || P->current_token.ID == TOKEN_SLASH ||
           P->current_token.ID == TOKEN_PERCENT)
    {
        advance(P);
        parse_unary_expression(P);
    }
}

void parse_unary_expression(parser *P)
{
    if (P->current_token.ID == TOKEN_MINUS || P->current_token.ID == TOKEN_EXCLAMATION ||
        P->current_token.ID == TOKEN_TILDE || P->current_token.ID == TOKEN_INC ||
        P->current_token.ID == TOKEN_DEC)
    {
        advance(P);
        parse_unary_expression(P);
    }
    else
    {
        parse_primary_expression(P);
        if (P->current_token.ID == TOKEN_INC || P->current_token.ID == TOKEN_DEC)
        {
            advance(P);
        }
    }
}

void parse_primary_expression(parser *P)
{
    if (P->current_token.ID == TOKEN_INT || P->current_token.ID == TOKEN_REAL ||
        P->current_token.ID == TOKEN_STRING || P->current_token.ID == TOKEN_CHAR ||
        P->current_token.ID == TOKEN_HEX)
    {
        advance(P);
    }
    else if (P->current_token.ID == TOKEN_IDENTIFIER)
    {
        advance(P);
        // First, handle all postfix operators including function calls
        while (P->current_token.ID == TOKEN_DOT || P->current_token.ID == TOKEN_LBRACKET || P->current_token.ID == TOKEN_LPAREN)
        {
            if (P->current_token.ID == TOKEN_DOT)
            {
                advance(P);
                if (P->current_token.ID != TOKEN_IDENTIFIER)
                {
                    fprintf(stderr, "Parser error in file %s line %d at text %s: Expected identifier after '.'\n",
                            P->filename, P->current_token.lineno, P->current_token.attrb);
                    remove(P->outfilename);
                    exit(1);
                }
                advance(P);
            }
            else if (P->current_token.ID == TOKEN_LBRACKET)
            {
                advance(P);
                parse_assignment_expression(P);
                match(P, TOKEN_RBRACKET);
            }
            else if (P->current_token.ID == TOKEN_LPAREN)
            {
                advance(P);
                if (P->current_token.ID != TOKEN_RPAREN)
                {
                    while (true)
                    {
                        parse_assignment_expression(P); // Parse each argument fully
                        if (P->current_token.ID == TOKEN_COMMA)
                        {
                            advance(P);
                        }
                        else
                        {
                            break;
                        }
                    }
                }
                match(P, TOKEN_RPAREN);
            }
        }
    }
    else if (P->current_token.ID == TOKEN_LPAREN)
    {
        advance(P);
        if (P->current_token.ID == TOKEN_TYPE)
        {
            advance(P);
            match(P, TOKEN_RPAREN);
            parse_assignment_expression(P);
        }
        else
        {
            parse_assignment_expression(P);
            match(P, TOKEN_RPAREN);
        }
    }
    else
    {
        fprintf(stderr, "Parser error in file %s line %d at text %s: Expected identifier (within expression)\n",
                P->filename, P->current_token.lineno, P->current_token.attrb);
        remove(P->outfilename);
        exit(1);
    }
}