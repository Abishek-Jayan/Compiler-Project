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

    printf("Advancing from token ID %d, text '%s'\n",
           P->current_token.ID, P->current_token.attrb ? P->current_token.attrb : "");
    if (P->current_token.attrb)
    {
        free(P->current_token.attrb);
    }
    getNextToken(P->L);
    P->current_token = P->L->current;
    printf("Advanced to token ID %d, text '%s'\n",
           P->current_token.ID, P->current_token.attrb ? P->current_token.attrb : "");
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
        if (P->current_token.ID == TOKEN_TYPE)
        {
            parse_declaration(P);
        }
        else
        {
            fprintf(stderr, "Parser error in file %s in line %d: Expected type name\n", P->filename, P->current_token.lineno);
            remove(P->outfilename);
            exit(1);
        }
    }
}

// Checks if current token is a function or a variable, calling the corresponding function for each
void parse_declaration(parser *P)
{
    match(P, TOKEN_TYPE);
    if (P->current_token.ID != TOKEN_IDENTIFIER)
    {
        fprintf(stderr, "Parser error in file %s line %d: Expected identifier\n", P->filename, P->current_token.lineno);
        remove(P->outfilename);
        exit(1);
    }
    char *ident = strdup(P->current_token.attrb);
    unsigned line = P->current_token.lineno;
    advance(P);
    if (P->current_token.ID == TOKEN_LPAREN)
    {
        fprintf(P->output, "File %s Line %d: function %s\n", P->filename, line, ident);
        parse_function_definition(P);
    }
    else
    {
        parse_variable_list(P, ident, line, P->is_inside_function ? "local variable" : "global variable");
        while (P->current_token.ID == TOKEN_COMMA)
        {
            advance(P);
            if (P->current_token.ID != TOKEN_IDENTIFIER)
            {
                fprintf(stderr, "Parser error in file %s line %u: Expected identifier after comma\n", P->filename, P->current_token.lineno);
                remove(P->outfilename);
                exit(1);
            }
            free(ident);
            ident = strdup(P->current_token.attrb);
            line = P->current_token.lineno;
            advance(P);
            parse_variable_list(P, ident, line, P->is_inside_function ? "local variable" : "global variable");
        }
        match(P, TOKEN_SEMICOLON);
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
            fprintf(stderr, "Parser error in file %s line %d: Expected integer literal for array size\n", P->filename, P->current_token.lineno);
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
    match(P, TOKEN_LBRACE);
    P->is_inside_function = true;
    while (P->current_token.ID != TOKEN_RBRACE && P->current_token.ID != END)
    {
        if (P->current_token.ID == TOKEN_TYPE)
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

//
void parse_formal_parameter(parser *P)
{
    if (P->current_token.ID != TOKEN_TYPE)
    {
        fprintf(stderr, "Parser error in file %s line %d: Expected type name for parameter\n", P->filename, P->current_token.lineno);
        remove(P->outfilename);
        exit(1);
    }

    advance(P);

    if (P->current_token.ID != TOKEN_IDENTIFIER)
    {
        fprintf(stderr, "Parser error in file %s line %d: Expected identifier for parameter\n", P->filename, P->current_token.lineno);
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
        if (P->current_token.ID == TOKEN_TYPE)
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
        if (P->current_token.ID == TOKEN_LPAREN)
        {
            advance(P);
            if (P->current_token.ID != TOKEN_RPAREN)
            {
                do
                {
                    parse_assignment_expression(P);
                    if (P->current_token.ID == TOKEN_COMMA)
                    {
                        advance(P);
                    }
                    else
                    {
                        break;
                    }
                } while (true);
            }
            match(P, TOKEN_RPAREN);
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
        fprintf(stderr, "Parser error in file %s line %d: Expected primary expression\n", P->filename, P->current_token.lineno);
        remove(P->outfilename);
        exit(1);
    }
}