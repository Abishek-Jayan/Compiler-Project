#ifndef PARSER_AST_H
#define PARSER_AST_H

#include "lexer.h"
#include <stdbool.h>

// Type System
typedef enum {
    BASE_VOID,
    BASE_CHAR,
    BASE_INT,
    BASE_FLOAT,
    BASE_STRUCT  // For user–defined structs
} BaseType;

typedef struct Type {
    BaseType base;
    bool isConst;            // true if declared const
    bool isArray;            // true if the variable is an array
    char structName[64];     // valid only if base==BASE_STRUCT
} Type;

/* Format a type as a string (e.g. "const struct pair[]" or "int") */
void format_type(const Type *t, char *outStr, size_t outSize);

/* Expression kinds */
typedef enum {
    EXPR_LITERAL,    // fixed values like 5 or 'a'
    EXPR_IDENTIFIER, // variable name like x
    EXPR_BINARY,     // two things combined, eg: x + y
    EXPR_UNARY,      // Single thing modified eg: -x, --x
    EXPR_ASSIGN,      // assignment operator
    EXPR_CAST,         // type cast 
    EXPR_CALL,        // function call
    EXPR_INDEX,       // array indexing
    EXPR_MEMBER,      // struct member selection
    EXPR_TERNARY      // 1 line if statement eg: x > 0 ? 1 : 0.
} ExprKind;

typedef enum {
    OP_PLUS, OP_MINUS, OP_MUL, OP_DIV, OP_MOD,
    OP_EQ, OP_NE, OP_LT, OP_LE, OP_GT, OP_GE,
    OP_AND, OP_OR,
    OP_NOT, OP_NEG, OP_INC, OP_DEC, OP_TILDE,
    OP_ASSIGN,
    OP_CAST, // explicit cast (e.g. (int)x)
    OP_PLUS_ASSIGN,
    OP_MINUS_ASSIGN,
    OP_MUL_ASSIGN,
    OP_DIV_ASSIGN,       
} Operator;

// Expression node
typedef struct Expression {
    ExprKind kind;
    int lineno;          
    Type exprType;       // computed type

    // Operator kind, if applicable (for binary/unary, cast, assignment, call, member)
    Operator op;

    char value[128];

    // For function calls: number of arguments, array of pointers 
    int numArgs;
    struct Expression **args;

    // For binary/unary/assignment/member/index expressions 
    struct Expression *left;
    struct Expression *right;
    
    // For member selection, store the member name
    char memberName[64];
} Expression;

// Statement kinds
typedef enum {
    STMT_DECL,        
    STMT_EXPR,        
    STMT_RETURN,      
    STMT_COMPOUND     // compound statement (a block)
} StmtKind;

// Declaration kinds are embedded in STMT_DECL.
typedef struct Declaration {
    Type declType;       // declared type including array or const info
    char name[64];       // variable name
    bool initialized;    // true if an initializer is provided
    Expression *init;    // initializer expression (may be NULL)
} Declaration;


// Function prototype / definition
typedef struct Function {
    char name[64];
    Type returnType;
    int numParams;
    Declaration **params;
    struct Statement *body;      
    bool defined;         // true if function defined, false if only prototype
    struct Function *next;
} Function;

// Statement node 
typedef struct Statement {
    StmtKind kind;
    int lineno;        
    union {
        Declaration decl;
        Expression *expr;
        Function *func;
    } u;
    // For compound statements (block), store an array of statements
    int numStmts;
    struct Statement **stmts;
} Statement;



// Struct definition for user-defined structs
typedef struct StructDef {
    char name[64];
    int numMembers;
    Declaration **members;  // array of member declarations
    bool isGlobal;          // global or local (error if duplicate names within same scope)
    struct StructDef *next;
} StructDef;


#define MAX_LOOKAHEAD 3 // Enough for type, identifier, and one more token (e.g., '(', '{', ';')

typedef struct LookaheadBuffer {
    token tokens[MAX_LOOKAHEAD];
    int count; // Number of tokens in buffer
} LookaheadBuffer;


// Parser Function Prototypes 
Statement *parser_statement(lexer *L, LookaheadBuffer *buf, bool isGlobal, bool isConst);
Statement *parse_compound(lexer *L);
Expression *parse_expression(lexer *L);
Expression *parse_assignment(lexer *L);
Expression *parse_ternary(lexer *L);
Expression *parse_logicalOr(lexer *L);
Expression *parse_logicalAnd(lexer *L);
Expression *parse_equality(lexer *L);
Expression *parse_relational(lexer *L);
Expression *parse_additive(lexer *L);
Expression *parse_multiplicative(lexer *L);
Expression *parse_unary(lexer *L);
Expression *parse_primary(lexer *L);

// Declaration parsing 
Statement *parser_declaration(lexer *L, LookaheadBuffer *buf, bool isGlobal, bool isConst);

// Parsing function prototypes and struct declarations
Statement *parse_function_declaration(lexer *L, LookaheadBuffer *buf);
Statement *parse_struct(lexer *L, LookaheadBuffer *buf);
Statement **parse_program(lexer *L, int *stmtCount);


// Utility initialization routines 
void init_symbol_tables();
void type_check_statement(Statement *stmt, const char *filename, FILE *output);
int parse_type_tokens(lexer *L, LookaheadBuffer *buf, token *typeTokens, int *typeTokenCount);

#endif
