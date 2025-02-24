#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <stdbool.h>
#include <stdint.h>

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


void lexer(FILE *input, char *filename, FILE *output);
int getNextToken(FILE *input, FILE *output, char *lexeme, int *lineNumber);
void printToken(FILE *output, char *filename, int *lineNumber, int token, const char *lexeme);
bool isKeyword(const char *lexeme);
bool isType(const char *lexeme);
bool isOperator(const char *lexeme);
bool isSymbol(char c);
bool isWhitespace(char c);
bool isStringLiteral(FILE *input, char *lexeme, int *lineNumber);
bool isCharLiteral(FILE *input, char *lexeme, int *lineNumber);
int isNumber(FILE *input, char *lexeme);
bool isIdentifier(FILE *input, char *lexeme);
bool isHexNumber(FILE *input, char *lexeme);
int getKeywordToken(const char *lexeme);
int getOperatorToken(const char *lexeme);

void lexer(FILE *input, char *filename, FILE *output)
{
    char lexeme[1024];
    int lineNumber = 1;
    int token;
    while ((token = getNextToken(input, output, lexeme, &lineNumber)) != 0)
    {
        if (token == -1)
        {
            break;
        }
        printToken(output, filename, &lineNumber, token, lexeme);
    }
}

int getNextToken(FILE *input, FILE *output, char *lexeme, int *lineNumber)
{
    char c = fgetc(input);
    while (isWhitespace(c))
    {
        if (c == '\n')
        {
            (*lineNumber)++;
        }
        c = fgetc(input);
    }
    if (c == EOF)
    {
        return 0;
    }
    if (c == '#')
    {
        char directive[32];
        int i = 0;
        while (!isspace(c = fgetc(input)) && c != EOF && i < 31)
        {
            directive[i++] = c;
        }
        directive[i] = '\0';
        if (strcmp(directive, "include") == 0)
        {
            while (isspace(c = fgetc(input)) && c != '\n' && c != EOF)
                ;
            if (c == '"')
            {
                char incFilename[256];
                i = 0;
                while ((c = fgetc(input)) != '"' && c != EOF && i < 255)
                {
                    incFilename[i++] = c;
                }
                incFilename[i] = '\0';
                FILE *incFile = fopen(incFilename, "r");
                if (!incFile)
                {
                    fprintf(stderr, "Lexer error: Cannot open include file %s at line %d\n", incFilename, *lineNumber);
                    return -1;
                }
                lexer(incFile, incFilename, output);
                fclose(incFile);
                return getNextToken(input, output, lexeme, lineNumber);
            }
        }
        while ((c = fgetc(input)) != '\n' && c != EOF)
            ;
        (*lineNumber)++;
        return getNextToken(input, output, lexeme, lineNumber);
    }

    if (c == '/')
    {
        char nextChar = fgetc(input);
        if (nextChar == '/')
        {
            // Single-line comment
            while ((c = fgetc(input)) != '\n' && c != EOF);
            if (c == '\n')
            {
                (*lineNumber)++;
            }
            return getNextToken(input, output, lexeme, lineNumber);
            ;
        }
        else if (nextChar == '*')
        {
            // Multi-line comment
            while (true)
            {
                c = fgetc(input);
                if (c == '\n')
                {
                    (*lineNumber)++;
                }
                if (c == EOF)
                {
                    fprintf(stderr, "Lexer error: Unclosed multi-line comment at line %d\n", *lineNumber);
                    return -1;
                }
                if (c == '*')
                {
                    char next = fgetc(input);
                    if (next == '/')
                    {
                        return getNextToken(input, output, lexeme, lineNumber); // End of multi-line comment
                    }
                    else
                    {
                        ungetc(next, input); 
                    }
                }
            }
        }
        else
        {
            ungetc(nextChar, input); 
            lexeme[0] = c;
            lexeme[1] = '\0';
            return c;
        }
    }

    if (c == '"')
    {
        if (isStringLiteral(input, lexeme, lineNumber))
        {
            return TOKEN_STRING;
        }
    }

    if (c == '\'')
    {
        if (isCharLiteral(input, lexeme, lineNumber))
        {
            return TOKEN_CHAR;
        }
    }

    if (isdigit(c) || c == '.')
    {
        ungetc(c, input);
        if (isHexNumber(input, lexeme))
        {
            return TOKEN_HEX;
        }
        else
        {
            return isNumber(input, lexeme);
        }
    }

    if (isalpha(c) || c == '_')
    {
        ungetc(c, input);
        if (isIdentifier(input, lexeme))
        {
            if (isKeyword(lexeme))
            {
                return getKeywordToken(lexeme);
            }
            else if (isType(lexeme))
            {
                return TOKEN_TYPE;
            }
            else
            {
                return TOKEN_IDENTIFIER;
            }
        }
    }

    if (isSymbol(c))
    {
        lexeme[0] = c;
        lexeme[1] = '\0';

        char next = fgetc(input);
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
            lexeme[1] = next;
            lexeme[2] = '\0';
        }
        else
        {
            ungetc(next, input);
        }
        if (isOperator(lexeme))
        {
            int token = getOperatorToken(lexeme);
            if (token != -1)
            {
                return token;
            }
        }
        return c;
    }
    fprintf(stderr, "Lexer error: Unknown character '%c'\n", c);
    return -1;
}

void printToken(FILE *output, char *filename, int *lineNumber, int token, const char *lexeme)
{
    fprintf(output, "File %s Line %d Token %d Text %s\n", filename, *lineNumber, token, lexeme);
}

bool isKeyword(const char *lexeme)
{
    const char *keywords[] = {
        "const", "struct", "for", "while", "do", "if", "else", "break", "continue", "return", "switch", "case", "default"};
    for (size_t i = 0; i < sizeof(keywords) / sizeof(keywords[0]); i++)
    {
        if (strcmp(lexeme, keywords[i]) == 0)
        {
            return true;
        }
    }
    return false;
}

bool isType(const char *lexeme)
{
    const char *types[] = {
        "void", "char", "int", "float"};
    for (size_t i = 0; i < sizeof(types) / sizeof(types[0]); i++)
    {
        if (strcmp(lexeme, types[i]) == 0)
        {
            return true;
        }
    }
    return false;
}

bool isOperator(const char *lexeme)
{
    const char *operators[] = {
        "==", "!=", ">=", "<=", "++", "--", "||", "&&", "+=", "-=", "*=", "/="};
    for (size_t i = 0; i < sizeof(operators) / sizeof(operators[0]); i++)
    {
        if (strcmp(lexeme, operators[i]) == 0)
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

bool isWhitespace(char c)
{
    return c == ' ' || c == '\t' || c == '\n' || c == '\r';
}

bool isStringLiteral(FILE *input, char *lexeme, int *lineNumber)
{
    int i = 0;
    int c;
    while ((c = fgetc(input)) != '"' && c != EOF)
    {
        if (c == '\\')
        { 
            c = fgetc(input);
            switch (c)
            {
            case 'n':
                lexeme[i++] = '\\';
                lexeme[i++] = 'n';
                break;
            case 't':
                lexeme[i++] = '\\';
                lexeme[i++] = 't';
                break;
            case '"':
                lexeme[i++] = '"';
                break;
            // Add other cases (e.g., \', \\, etc.)
            default:
                lexeme[i++] = c;
                break;
            }
        }
        else
        {
            lexeme[i++] = c;
        }
        if (i >= 1023)
        {
            fprintf(stderr, "Lexer error: String literal too long at line %d\n", *lineNumber);
            return false;
        }
    }

    if (c == EOF)
    {
        fprintf(stderr, "Lexer error: Unterminated string literal at line %d\n", *lineNumber);
        return false;
    }
    lexeme[i] = '\0';
    return true;
}

bool isCharLiteral(FILE *input, char *lexeme, int *lineNumber)
{
    int i = 0;
    char c;
    while ((c = fgetc(input)) != '\'' && c != EOF)
    {
        if (c == '\\')
        { 
            lexeme[i++] = c;
            c = fgetc(input);
            if (c == EOF)
                break;
        }
        lexeme[i++] = c;
        if (i >= 1023)
        { 
            fprintf(stderr, "Lexer error: Char literal exceeds size limit at line %d\n", *lineNumber);
            return false;
        }
    }
    if (c != '\'')
    {
        return false; // error: unclosed string literal
    }
    lexeme[i] = '\0';
    return true;
}

int isNumber(FILE *input, char *lexeme)
{
    int i = 0;
    bool isReal = false;
    char c = fgetc(input);
    while (isdigit(c))
    {
        lexeme[i++] = c;
        c = fgetc(input);
    }
    if (c == '.')
    {
        isReal = true;
        lexeme[i++] = c;
        c = fgetc(input);
        while (isdigit(c))
        {
            lexeme[i++] = c;
            c = fgetc(input);
        }
    }
    if (c == 'e' || c == 'E')
    {
        isReal = true;
        lexeme[i++] = c;
        c = fgetc(input);
        if (c == '+' || c == '-')
        { 
            lexeme[i++] = c;
            c = fgetc(input);
        }
        while (isdigit(c))
        {
            lexeme[i++] = c;
            c = fgetc(input);
        }
    }
    lexeme[i] = '\0';
    ungetc(c, input);
    return isReal ? TOKEN_REAL : TOKEN_INT;
}

bool isIdentifier(FILE *input, char *lexeme)
{
    int i = 0;
    int c;
    while ((c = fgetc(input)) != EOF && (isalnum(c) || c == '_'))
    {
        if (i < 1023)
        { 
            lexeme[i++] = c;
        }
        else
        {
            fprintf(stderr, "Identifier too long\n");
            return false;
        }
    }
    lexeme[i] = '\0';
    ungetc(c, input);
    return true;
}

bool isHexNumber(FILE *input, char *lexeme)
{
    int i = 0;
    char c = fgetc(input);
    if (c == '0')
    {
        char next = fgetc(input);
        if (next == 'x' || next == 'X')
        {
            int validHex = 0;
            while (isxdigit(c = fgetc(input)))
            {
                lexeme[i++] = c;
                validHex = 1;
            }
            if (!validHex)
            {
                ungetc(c, input);
                return false; // Invalid hex literal (e.g., "0x")
            }
            lexeme[i] = '\0';
            return true;
        }
        else
        {
            ungetc(next, input);
        }
    }
    ungetc(c, input);
    return false;
}

int getKeywordToken(const char *lexeme)
{
    const char *keywords[] = {
        "const", "struct", "for", "while", "do", "if", "else", "break", "continue", "return", "switch", "case", "default"};
    int tokens[] = {
        TOKEN_CONST, TOKEN_STRUCT, TOKEN_FOR, TOKEN_WHILE, TOKEN_DO, TOKEN_IF, TOKEN_ELSE, TOKEN_BREAK, TOKEN_CONTINUE, TOKEN_RETURN, TOKEN_SWITCH, TOKEN_CASE, TOKEN_DEFAULT};
    for (size_t i = 0; i < sizeof(keywords) / sizeof(keywords[0]); i++)
    {
        if (strcmp(lexeme, keywords[i]) == 0)
        {
            return tokens[i];
        }
    }
    return -1;
}

int getOperatorToken(const char *lexeme)
{
    const char *operators[] = {
        "==", "!=", ">=", "<=", "++", "--", "||", "&&", "+=", "-=", "*=", "/="};
    int tokens[] = {
        TOKEN_EQ, TOKEN_NE, TOKEN_GE, TOKEN_LE, TOKEN_INC, TOKEN_DEC, TOKEN_OR, TOKEN_AND, TOKEN_ADD_ASSIGN, TOKEN_SUB_ASSIGN, TOKEN_MUL_ASSIGN, TOKEN_DIV_ASSIGN};
    for (size_t i = 0; i < sizeof(operators) / sizeof(operators[0]); i++)
    {
        if (strcmp(lexeme, operators[i]) == 0)
        {
            return tokens[i];
        }
    }
    return -1;
}