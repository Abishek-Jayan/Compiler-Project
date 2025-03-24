#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <stdbool.h>
#include <stdint.h>
#include "lexer.h"

bool isKeyword(char *checking_string);
bool isType(char *checking_string);
bool isOperator(char *checking_string);
bool isSymbol(char c);
bool isIdentifier(lexer *L, char *checking_string);
int getKeywordToken(lexer *L, char *checking_string);
int getOperatorToken(lexer *L, char *checking_string);
int getSymbolToken(char c);

void init_lexer(lexer *L, char *infilename, char *outfilename)
{
    if (!L)
        return; // If lexer object is null
    L->filename = infilename;
    L->infile = fopen(infilename, "r");
    L->outfile = fopen(outfilename, "w");
    L->outfilename = outfilename;
    L->current.attrb = NULL;
    L->lineno = 1;
    getNextToken(L);
}

/*
Set the current token to the next token on the input stream
If we encounter eof, use end
*/
void getNextToken(lexer *L)
{

    int c;
    int is_single_comment = 0;
    int is_multi_comment = 0;
    while (true)
    {
        c = fgetc(L->infile);
        // Case 1: End of File
        if (c == EOF)
        {
            // Case 1.1: If multiline comment was started but not closed
            if (is_multi_comment)
            {
                fprintf(stderr, "Lexer error in file %s line %d: No closing argument.", L->filename, L->lineno);
                exit(1);
            }

            L->current.ID = END;
            L->current.lineno = L->lineno;
            return;
        }

        // Case 2: WhiteSpace
        if ((c == ' ') || (c == '\t') || (c == '\r'))
        {
            continue;
        }

        // Case 3: NewLine
        if (c == '\n')
        {
            L->lineno++;
            is_single_comment = 0;
            continue;
        }

        // Case 4: Comments
        // Case 4.1: End of multiline comment
        if (c == '*')
        {
            int next = fgetc(L->infile);
            if (next == '/')
            {
                is_multi_comment = 0;
                continue;
            }
            ungetc(next, L->infile);
        }
        // Case 4.2: Inside a comment
        if (is_single_comment || is_multi_comment)
            continue;

        // Case 4.3: Start of Comment
        if (c == '/')
        {
            int next = fgetc(L->infile);

            // Case 4.3.1: Start of SingleLine Comments
            if (next == '/')
            {
                is_single_comment = 1;
                continue;
            }

            // Case 4.3.2: Start of Multiline Comments
            if (next == '*')
            {
                is_multi_comment = 1;
                continue;
            }

            // Else Case: Its just a division operator
            ungetc(next, L->infile);
            L->current.ID = TOKEN_SLASH;
            L->current.attrb = "/";
            return;
        }

        // Case 5: #include directives
        if (c == '#')
        {
            c = fgetc(L->infile);
            char checking_string[255];
            int i = 0;
            while (!((c == ' ') || (c == '\t') || (c == '\r')) && c != EOF && i < 253)
            {
                checking_string[i++] = c;
                c = fgetc(L->infile);
            }
            checking_string[i] = '\0';
            if (strcmp(checking_string, "include") == 0)
            {
                c = fgetc(L->infile);
                while (!((c == ' ') || (c == '\t') || (c == '\r')) && c != EOF && c != '\n')
                {
                    c = fgetc(L->infile);
                }

                if (c == '"')
                {
                    i = 0;
                    c = fgetc(L->infile);
                    while (c != '"' && c != EOF && c != '\n' && i < 254)
                    {
                        checking_string[i++] = c;
                        c = fgetc(L->infile);
                    }
                    checking_string[i] = '\0';
                    FILE *incFile = fopen(checking_string, "r");
                    if (!incFile)
                    {
                        fprintf(stderr, "Lexer error in file %s line %d at text %s: Cannot open include file\n", L->filename, L->lineno, checking_string);
                        exit(1);
                    }
                    lexer P;
                    init_lexer(&P, checking_string, L->outfilename);
                    while (P.current.ID)
                    {
                        fprintf(L->outfile, "File %s Line %d Token %d Text %s\n", P.filename, P.lineno, P.current.ID, P.current.attrb);
                        getNextToken(&P);
                    }
                    fclose(incFile);
                }
            }
            continue;
        }

        L->current.lineno = L->lineno; // At this point, we have skipped past all comments and whitespace

        // Case 6: String Literal
        if (c == '"')
        {
            char checking_string[1024];

            int i = 0;
            while ((c = fgetc(L->infile)) != '"' && c != EOF)
            {
                if (c == '\\')
                {
                    c = fgetc(L->infile);
                    switch (c)
                    {
                    case 'n':
                        checking_string[i++] = '\\';
                        checking_string[i++] = 'n';
                        break;
                    case 't':
                        checking_string[i++] = '\\';
                        checking_string[i++] = 't';
                        break;
                    case 'r':
                        checking_string[i++] = '\\';
                        checking_string[i++] = 'r';
                        break;
                    case 'a':
                        checking_string[i++] = '\\';
                        checking_string[i++] = 'a';
                        break;
                    case 'b':
                        checking_string[i++] = '\\';
                        checking_string[i++] = 'b';
                        break;
                    case '\\':
                        checking_string[i++] = '\\';
                        break;
                    case '"':
                        checking_string[i++] = '"';
                        break;
                    default:
                        fprintf(stderr, "Lexer error in file %s line %d at text \\%s: Invalid escape sequence\n", L->filename, L->lineno, checking_string);
                        exit(1);
                    }
                }
                else
                {
                    checking_string[i++] = c;
                }
                if (i >= 1023)
                {
                    fprintf(stderr, "Lexer error in file %s line %d at text %s: String literal too long\n", L->filename, L->current.lineno, checking_string);
                    exit(1);
                }
            }
            if (c == EOF)
            {
                fprintf(stderr, "Lexer error in file %s line %d at text %s: Unterminated string literal\n", L->filename, L->current.lineno, checking_string);
                exit(1);
            }
            checking_string[i] = '\0';
            L->current.ID = TOKEN_STRING;
            L->current.attrb = strdup(checking_string);
            return;
        }

        // Case 7: Character Literal
        if (c == '\'')
        {
            char checking_string[5];

            int i = 0;
            c = fgetc(L->infile);
            if (c == '\\')
            {
                checking_string[i++] = c;
                c = fgetc(L->infile);
                switch (c)
                {
                case 'a':
                case 'b':
                case '\'':
                case 'r':
                case 'n':
                case '\\':
                    checking_string[i++] = c;
                    break;
                default:
                    fprintf(stderr, "Lexer error in file %s line %d at text \\%s: Invalid escape sequence\n", L->filename, L->current.lineno, checking_string);
                    exit(1);
                }
            }
            else
            {
                checking_string[i++] = c;
            }
            c = fgetc(L->infile);
            if (c != '\'')
            {
                fprintf(stderr, "Lexer error in file %s line %d at text %s: Unterminated char literal\n", L->filename, L->current.lineno, checking_string);
                exit(1);
            }
            checking_string[i] = '\0';
            L->current.ID = TOKEN_CHAR;
            L->current.attrb = strdup(checking_string);
            return;
        }

        // Case 8: Integer Literal
        if (isdigit(c) || c == '.')
        {
            char checking_string[48];
            int i = 0;
            checking_string[i++] = c;
            // Case 8.1 Hexadecimal
            if (c == '0')
            {
                c = fgetc(L->infile);
                if (c == 'x' || c == 'X')
                {
                    checking_string[i++] = c;
                    while (isxdigit(c = fgetc(L->infile)))
                    {
                        if (i < 47)
                            checking_string[i++] = c;
                    }
                    if (c != EOF)
                        ungetc(c, L->infile);
                    L->current.ID = TOKEN_HEX;
                    L->current.attrb = strdup(checking_string);
                    L->current.lineno = L->lineno;
                    return;
                }
                else
                {
                    ungetc(c, L->infile);
                }
            }
            // Case 8.2: Number
            while ((c = fgetc(L->infile)) != EOF && (isdigit(c) || c == '.'))
            {
                if (i < 47)
                    checking_string[i++] = c;
            }
            checking_string[i] = '\0';
            if (c != EOF)
                ungetc(c, L->infile);
            L->current.ID = (strchr(checking_string, '.') ? TOKEN_REAL : TOKEN_INT);
            L->current.attrb = strdup(checking_string);
            L->current.lineno = L->lineno;
            return;
        }

        // Case 9: Identifiers
        if (isalpha(c) || c == '_')
        {
            ungetc(c, L->infile);
            char checking_string[48];
            if (isIdentifier(L, checking_string))
            {
                if (isKeyword(checking_string))
                {
                    L->current.ID = getKeywordToken(L, checking_string);
                    L->current.attrb = strdup(checking_string);
                    return;
                }
                else if (isType(checking_string))
                {
                    L->current.ID = TOKEN_TYPE;
                    L->current.attrb = strdup(checking_string);
                    return;
                }
                else
                {
                    L->current.ID = TOKEN_IDENTIFIER;
                    L->current.attrb = strdup(checking_string);
                    return;
                }
            }
        }

        // Case 10: Symbols
        if (isSymbol(c))
        {
            char checking_string[48];
            checking_string[0] = c;
            checking_string[1] = '\0';

            char next = fgetc(L->infile);
            if ((c == '=' && next == '=') ||
                (c == '!' && next == '=') ||
                (c == '>' && next == '=') ||
                (c == '<' && next == '=') ||
                (c == '+' && (next == '+' || next == '=')) ||
                (c == '-' && (next == '-' || next == '=')) ||
                (c == '|' && next == '|') ||
                (c == '&' && next == '&') ||
                (c == '*' && next == '=') ||
                (c == '/' && next == '='))
            {
                checking_string[1] = next;
                checking_string[2] = '\0';
            }
            else
            {
                ungetc(next, L->infile);
                L->current.ID = getSymbolToken(c);
                L->current.attrb = strdup(checking_string);
                return;
            }
            if (isOperator(checking_string))
            {
                int token = getOperatorToken(L, checking_string);
                if (token != -1)
                {
                    L->current.ID = token;
                    L->current.attrb = strdup(checking_string);
                    return;
                }
            }
        }
        fprintf(stderr, "Lexer error in file %s line %d at text %c: Unknown character\n", L->filename, L->current.lineno, c);
        exit(1);
    }
}

bool isKeyword(char *checking_string)
{
    const char *keywords[] = {
        "const", "struct", "for", "while", "do", "if", "else", "break", "continue", "return", "switch", "case", "default"};
    for (size_t i = 0; i < sizeof(keywords) / sizeof(keywords[0]); i++)
    {
        if (strcmp(checking_string, keywords[i]) == 0)
        {
            return true;
        }
    }
    return false;
}

bool isType(char *checking_string)
{
    const char *types[] = {
        "void", "char", "int", "float"};
    for (size_t i = 0; i < sizeof(types) / sizeof(types[0]); i++)
    {
        if (strcmp(checking_string, types[i]) == 0)
        {
            return true;
        }
    }
    return false;
}

bool isOperator(char *checking_string)
{
    const char *operators[] = {
        "==", "!=", ">=", "<=", "++", "--", "||", "&&", "+=", "-=", "*=", "/="};
    for (size_t i = 0; i < sizeof(operators) / sizeof(operators[0]); i++)
    {
        if (strcmp(checking_string, operators[i]) == 0)
        {
            return true;
        }
    }
    return false;
}

bool isSymbol(char c)
{
    const char symbols[] = {
        '!', '%', '&', '(', ')', '*', '+', ',', '-', '.', '/', ':', ';', '<', '=', '>', '?', '[', ']', '{', '}', '~'};
    for (size_t i = 0; i < sizeof(symbols) / sizeof(symbols[0]); i++)
    {
        if (c == symbols[i])
        {
            return true;
        }
    }
    return false;
}



bool isIdentifier(lexer *L, char *checking_string)
{
    int i = 0;
    int c;
    while ((c = fgetc(L->infile)) != EOF && (isalnum(c) || c == '_'))
    {
        if (i < 48)
        {
            checking_string[i++] = c;
        }
        else
        {
            fprintf(stderr, "Lexer error in file %s line %d at text %s: Identifier too long\n", L->filename, L->lineno, checking_string);
            return false;
        }
    }
    checking_string[i] = '\0';
    ungetc(c, L->infile);
    return true;
}



int getSymbolToken(char c)
{
    const char symbols[] = {
        '!', '%', '&', '(', ')', '*', '+', ',', '-', '.', '/', ':', ';', '<', '=', '>', '?', '[', ']', '{', '}', '|', '~'};
    int tokens[] = {
        TOKEN_EXCLAMATION, TOKEN_PERCENT, TOKEN_AMPERSAND, TOKEN_LPAREN, TOKEN_RPAREN, TOKEN_ASTERISK,
        TOKEN_PLUS, TOKEN_COMMA, TOKEN_MINUS, TOKEN_DOT, TOKEN_SLASH, TOKEN_COLON, TOKEN_SEMICOLON,
        TOKEN_LESS, TOKEN_EQUAL, TOKEN_GREATER, TOKEN_QUESTION, TOKEN_LBRACKET, TOKEN_RBRACKET,
        TOKEN_LBRACE, TOKEN_RBRACE, TOKEN_PIPE, TOKEN_TILDE};

    for (size_t i = 0; i < sizeof(symbols) / sizeof(symbols[0]); i++)
    {
        if (c == symbols[i])
        {
            return tokens[i];
        }
    }

    fprintf(stderr, "Lexer error: Invalid symbol '%c' encountered.\n", c);
    exit(1);
}

int getKeywordToken(lexer *L, char *checking_string)
{
    const char *keywords[] = {
        "const", "struct", "for", "while", "do", "if", "else", "break", "continue", "return", "switch", "case", "default"};
    int tokens[] = {
        TOKEN_CONST, TOKEN_STRUCT, TOKEN_FOR, TOKEN_WHILE, TOKEN_DO, TOKEN_IF, TOKEN_ELSE, TOKEN_BREAK, TOKEN_CONTINUE, TOKEN_RETURN, TOKEN_SWITCH, TOKEN_CASE, TOKEN_DEFAULT};
    for (size_t i = 0; i < sizeof(keywords) / sizeof(keywords[0]); i++)
    {
        if (strcmp(checking_string, keywords[i]) == 0)
        {
            return tokens[i];
        }
    }
    fprintf(stderr, "Lexer error in file %s line %d at text %s: Invalid keyword\n", L->filename, L->lineno, checking_string);
    exit(1);
}

int getOperatorToken(lexer *L, char *checking_string)
{
    const char *operators[] = {
        "==", "!=", ">=", "<=", "++", "--", "||", "&&", "+=", "-=", "*=", "/="};
    int tokens[] = {
        TOKEN_EQ, TOKEN_NE, TOKEN_GE, TOKEN_LE, TOKEN_INC, TOKEN_DEC, TOKEN_OR, TOKEN_AND, TOKEN_ADD_ASSIGN, TOKEN_SUB_ASSIGN, TOKEN_MUL_ASSIGN, TOKEN_DIV_ASSIGN};
    for (size_t i = 0; i < sizeof(operators) / sizeof(operators[0]); i++)
    {
        if (strcmp(checking_string, operators[i]) == 0)
        {
            return tokens[i];
        }
    }
    fprintf(stderr, "Lexer error in file %s line %d at text %s: Invalid operator\n", L->filename, L->lineno, checking_string);
    exit(1);
}