#include <stdio.h> // For FILE type
#ifndef LEXER_H
#define LEXER_H


#define END 0

// Token types
#define TOKEN_TYPE 301
#define TOKEN_CHAR 302
#define TOKEN_INT 303
#define TOKEN_REAL 304
#define TOKEN_STRING 305
#define TOKEN_IDENTIFIER 306
#define TOKEN_HEX 307

// Single character symbols
#define TOKEN_EXCLAMATION 33
#define TOKEN_PERCENT 37
#define TOKEN_AMPERSAND 38
#define TOKEN_LPAREN 40
#define TOKEN_RPAREN 41
#define TOKEN_ASTERISK 42
#define TOKEN_PLUS 43
#define TOKEN_COMMA 44
#define TOKEN_MINUS 45
#define TOKEN_DOT 46
#define TOKEN_SLASH 47
#define TOKEN_COLON 58
#define TOKEN_SEMICOLON 59
#define TOKEN_LESS 60
#define TOKEN_EQUAL 61
#define TOKEN_GREATER 62
#define TOKEN_QUESTION 63
#define TOKEN_LBRACKET 91
#define TOKEN_RBRACKET 93
#define TOKEN_LBRACE 123
#define TOKEN_RBRACE 125
#define TOKEN_TILDE 126

// Keywords
#define TOKEN_CONST 401
#define TOKEN_STRUCT 402
#define TOKEN_FOR 403
#define TOKEN_WHILE 404
#define TOKEN_DO 405
#define TOKEN_IF 406
#define TOKEN_ELSE 407
#define TOKEN_BREAK 408
#define TOKEN_CONTINUE 409
#define TOKEN_RETURN 410
#define TOKEN_SWITCH 411
#define TOKEN_CASE 412
#define TOKEN_DEFAULT 413

// Operators with two characters
#define TOKEN_EQ 351
#define TOKEN_NE 352
#define TOKEN_GE 353
#define TOKEN_LE 354
#define TOKEN_INC 355
#define TOKEN_DEC 356
#define TOKEN_OR 357
#define TOKEN_AND 358
#define TOKEN_ADD_ASSIGN 361
#define TOKEN_SUB_ASSIGN 362
#define TOKEN_MUL_ASSIGN 363
#define TOKEN_DIV_ASSIGN 364


typedef struct {
    unsigned ID; //Token ID
    char* attrb; // Token word
    unsigned lineno; //Token line number
} token;

typedef struct {
    char* filename;
    char* outfilename;
    unsigned lineno;
    FILE* infile;
    FILE* outfile;
    token current;
} lexer; //Tracks where I am in the lexer


void init_lexer(lexer *L, char *infilename, char *outfilename);

void getNextToken(lexer *L);

#endif