#include "parser_ast.h"
#include "lexer.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

char *inputfilename;

// Initialises a lookahead buffer to save lexer states
void init_lookahead(LookaheadBuffer *buf)
{
    buf->count = 0;
}

// Add a token to the buffer
void push_token(LookaheadBuffer *buf, token t)
{
    if (buf->count < MAX_LOOKAHEAD)
    {
        buf->tokens[buf->count++] = t;
    }
    else
    {
        fprintf(stderr, "Lookahead buffer full\n");
        exit(1);
    }
}

// Clear the buffer
void clear_lookahead(LookaheadBuffer *buf)
{
    buf->count = 0;
}

/* Variable symbol */
typedef struct VarSymbol
{
    char name[64];
    Type type;
    bool isGlobal;
    struct VarSymbol *next;
} VarSymbol;

VarSymbol *varSymbols = NULL;

/* Function symbol */
Function *funcSymbols = NULL;

/* Struct symbol */
StructDef *structSymbols = NULL;

void *my_malloc(size_t bytes)
{
    void *x = malloc(bytes);
    if (!x)
    {
        fprintf(stderr, "malloc failure\n");
        exit(1);
    }
    return x;
}

bool equal_types(const Type *a, const Type *b)
{
    if (a->base != b->base)
        return false;
    if (a->base == BASE_STRUCT && strcmp(a->structName, b->structName) != 0)
        return false;
    if (a->isArray != b->isArray)
        return false;
    return true;
}

// Automatic widening
bool can_widen(const Type *from, const Type *to)
{
    if (equal_types(from, to))
        return true;
    if (from->base == BASE_CHAR && to->base == BASE_INT)
        return true;
    if ((from->base == BASE_CHAR || from->base == BASE_INT) && to->base == BASE_FLOAT)
        return true;
    return false;
}

// Produce a widened type given from and to
void widen_expression(Expression *expr, const Type *target)
{
    expr->exprType = *target;
}

// Format a type into a human–readable string
void format_type(const Type *t, char *outStr, size_t outSize)
{
    char buf[128] = "";
    if (t->isConst)
        strcat(buf, "const ");
    if (t->base == BASE_STRUCT)
    {
        strcat(buf, "struct ");
        strcat(buf, t->structName);
    }
    else
    {
        switch (t->base)
        {
        case BASE_VOID:
            strcat(buf, "void");
            break;
        case BASE_CHAR:
            strcat(buf, "char");
            break;
        case BASE_INT:
            strcat(buf, "int");
            break;
        case BASE_FLOAT:
            strcat(buf, "float");
            break;
        default:
            strcat(buf, "unknown");
        }
    }
    if (t->isArray)
        strcat(buf, "[]");
    strncpy(outStr, buf, outSize - 1);
    outStr[outSize - 1] = '\0';
}

// Error reporting for type checking
void type_error(const char *filename, int lineno, const char *msg)
{
    fprintf(stderr, "Type checking error in file %s line %d: %s\n", filename, lineno, msg);
    exit(1);
}

/* Syntax error for parser */
void syntax_error(lexer *L, const char *expected)
{
    fprintf(stderr, "Syntax error in file %s line %u: Expected %s, but saw ",
            L->filename, L->lineno, expected);
    if (L->current.attrb)
        fprintf(stderr, "%s\n", L->current.attrb);
    else
        fprintf(stderr, "token %d\n", L->current.ID);
    exit(1);
}

void add_variable(const char *name, Type type, bool isGlobal)
{
    VarSymbol *vs = my_malloc(sizeof(VarSymbol));
    strncpy(vs->name, name, sizeof(vs->name) - 1);
    vs->type = type;
    vs->isGlobal = isGlobal;
    vs->next = varSymbols;
    varSymbols = vs;
}

VarSymbol *lookup_variable(const char *name)
{
    VarSymbol *cur = varSymbols;
    while (cur)
    {
        if (strcmp(cur->name, name) == 0)
            return cur;
        cur = cur->next;
    }
    return NULL;
}
Function *lookup_function(const char *name)
{
    Function *cur = funcSymbols;
    while (cur)
    {
        if (strcmp(cur->name, name) == 0)
            return cur;
        cur = cur->next;
    }
    return NULL;
}
StructDef *lookup_struct(const char *name)
{
    StructDef *cur = structSymbols;
    while (cur)
    {
        if (strcmp(cur->name, name) == 0)
            return cur;
        cur = cur->next;
    }
    return NULL;
}

// Initialize symbol tables
void init_symbol_tables()
{
    varSymbols = NULL;
    funcSymbols = NULL;
    structSymbols = NULL;
}

// Create a literal node (integer, float, char, string)
Expression *make_literal(const char *value, Type type, int lineno)
{
    Expression *node = my_malloc(sizeof(Expression));
    node->kind = EXPR_LITERAL;
    node->lineno = lineno;
    strncpy(node->value, value, sizeof(node->value) - 1);
    node->exprType = type;
    node->left = node->right = NULL;
    node->numArgs = 0;
    return node;
}

// Create an identifier node
Expression *make_identifier(const char *name, int lineno)
{
    Expression *node = my_malloc(sizeof(Expression));
    node->kind = EXPR_IDENTIFIER;
    node->lineno = lineno;
    strncpy(node->value, name, sizeof(node->value) - 1);

    // Check if it's a variable
    VarSymbol *vs = lookup_variable(name);
    if (vs)
    {
        node->exprType = vs->type;
    }
    else
    {
        // Check if it's a function
        Function *func = lookup_function(name);
        if (!func)
        {
            type_error(inputfilename, lineno, "Using undeclared variable or function");
        }
        node->exprType = func->returnType;
    }

    node->left = node->right = NULL;
    node->numArgs = 0;
    return node;
}

// Create a binary operator node
Expression *make_binary(Expression *left, Operator op, Expression *right, int lineno)
{
    Expression *node = my_malloc(sizeof(Expression));
    node->kind = EXPR_BINARY;
    node->lineno = lineno;
    node->op = op;
    node->left = left;
    node->right = right;
    node->numArgs = 0;
    if (left && right)
    {
        // Check for invalid void types in relational/logical operators
        if ((op == OP_EQ || op == OP_NE || op == OP_LT || op == OP_LE ||
             op == OP_GT || op == OP_GE || op == OP_AND || op == OP_OR) &&
            (left->exprType.base == BASE_VOID || right->exprType.base == BASE_VOID))
        {
            char leftType[22], rightType[22];
            format_type(&left->exprType, leftType, sizeof(leftType));
            format_type(&right->exprType, rightType, sizeof(rightType));
            char msg[256];
            snprintf(msg, sizeof(msg), "Invalid operation: %s %s %s",
                     leftType, op == OP_EQ ? "==" : op == OP_NE ? "!=" :
                     op == OP_LT ? "<" : op == OP_LE ? "<=" :
                     op == OP_GT ? ">" : op == OP_GE ? ">=" :
                     op == OP_AND ? "&&" : "||", rightType);
            type_error(inputfilename, lineno, msg);
        }
        if (!equal_types(&left->exprType, &right->exprType))
        {
            if (can_widen(&left->exprType, &right->exprType))
            {
                widen_expression(left, &right->exprType);
                node->exprType = right->exprType;
            }
            else if (can_widen(&right->exprType, &left->exprType))
            {
                widen_expression(right, &left->exprType);
                node->exprType = left->exprType;
            }
            else
            {
                type_error(inputfilename, lineno, "Type mismatch in binary operator");
            }
        }
        else
        {
            node->exprType = left->exprType;
        }
    }
    return node;
}

// Create a unary operator node
Expression *make_unary(Operator op, Expression *operand, int lineno)
{
    Expression *node = my_malloc(sizeof(Expression));
    node->kind = EXPR_UNARY;
    node->lineno = lineno;
    node->op = op;
    node->left = NULL;
    node->right = operand;
    node->numArgs = 0;
    node->exprType = operand->exprType;
    if ((op == OP_INC || op == OP_DEC))
    {
        if (operand->exprType.isConst)
            type_error(inputfilename, lineno, "Invalid operation on const item");
        if (operand->exprType.isArray)
        {
            char typeStr[128];
            format_type(&operand->exprType, typeStr, sizeof(typeStr));
            char msg[256];
            snprintf(msg, sizeof(msg), "Invalid operation: %s %s", typeStr, op == OP_INC ? "++" : "--");
            type_error(inputfilename, lineno, msg);
        }
    }
    return node;
}

// Create an assignment node (lvalue = rvalue)
Expression *make_assignment(Expression *lvalue, Expression *rvalue, int lineno, Operator op)
{
    Expression *node = my_malloc(sizeof(Expression));
    node->kind = EXPR_ASSIGN;
    node->lineno = lineno;
    node->op = op;
    node->left = lvalue;
    node->right = rvalue;
    node->numArgs = 0;
    if (lvalue->exprType.isConst)
        type_error(inputfilename, lineno, "Assignment to a const variable");
    if (lvalue->exprType.isArray)
        type_error(inputfilename, lineno, "Cannot assign to an array type");
    /* Check type compatibility (allow automatic widening) */
    if (!equal_types(&lvalue->exprType, &rvalue->exprType))
    {
        if (can_widen(&rvalue->exprType, &lvalue->exprType))
        {
            widen_expression(rvalue, &lvalue->exprType);
        }
        else
        {
            type_error(inputfilename, lineno, "Type mismatch in assignment");
        }
    }
    // Additional checks for compound assignments
    if (op != OP_ASSIGN)
    {
        // Ensure lvalue and rvalue are numeric types (int, float, char)
        if (lvalue->exprType.base != BASE_INT &&
            lvalue->exprType.base != BASE_FLOAT &&
            lvalue->exprType.base != BASE_CHAR)
        {
            type_error("input.c", lineno, "Compound assignment requires numeric type");
        }
    }
    node->exprType = lvalue->exprType;
    return node;
}

// Create a cast node: (castType) expression
Expression *make_cast(Type castType, Expression *expr, int lineno)
{
    Expression *node = my_malloc(sizeof(Expression));
    node->kind = EXPR_CAST;
    node->lineno = lineno;
    node->op = OP_CAST;
    node->left = expr;
    node->right = NULL;
    node->numArgs = 0;
    if (!((expr->exprType.base == BASE_INT || expr->exprType.base == BASE_FLOAT || expr->exprType.base == BASE_CHAR) &&
          (castType.base == BASE_INT || castType.base == BASE_FLOAT || castType.base == BASE_CHAR)))
        type_error(inputfilename, lineno, "Illegal cast");
    node->exprType = castType;
    return node;
}

// Create a function call node
Expression *make_call(Expression *funcExpr, Expression **args, int numArgs, int lineno)
{
    Expression *node = my_malloc(sizeof(Expression));
    node->kind = EXPR_CALL;
    node->lineno = lineno;
    node->left = funcExpr;
    node->numArgs = numArgs;
    node->args = args;

    Function *func = lookup_function(funcExpr->value);
    if (!func)
    {
        type_error(inputfilename, lineno, "Call to undeclared function");
    }

    if (func->numParams != numArgs)
    {
        type_error(inputfilename, lineno, "Incorrect number of arguments");
    }

    for (int i = 0; i < numArgs; i++)
    {
        char argType[50], paramType[50];
        format_type(&args[i]->exprType, argType, sizeof(argType));
        format_type(&func->params[i]->declType, paramType, sizeof(paramType));
        if (!equal_types(&args[i]->exprType, &func->params[i]->declType))
        {
            if (!can_widen(&args[i]->exprType, &func->params[i]->declType))
            {
                char msg[256];
                snprintf(msg, sizeof(msg), "In call to %s: Parameter #%d should be %s, was %s",
                         func->name, i + 1, paramType, argType);
                type_error("fcall7.c", lineno, msg);
            }
            widen_expression(args[i], &func->params[i]->declType);
        }
    }

    node->exprType = func->returnType;
    return node;
}

// Create an array indexing node: array[index]
Expression *make_index(Expression *arrayExpr, Expression *indexExpr, int lineno)
{
    Expression *node = my_malloc(sizeof(Expression));
    node->kind = EXPR_INDEX;
    node->lineno = lineno;
    node->left = arrayExpr;
    node->right = indexExpr;
    node->numArgs = 0;
    if (!arrayExpr->exprType.isArray)
        type_error(inputfilename, lineno, "Attempt to index a non–array type");
    if (indexExpr->exprType.base != BASE_INT)
        type_error(inputfilename, lineno, "Array index is not of integer type");
    node->exprType = arrayExpr->exprType;
    node->exprType.isArray = false;
    return node;
}

// Create a member selection node: expr.member
Expression *make_member(Expression *structExpr, const char *member, int lineno)
{
    Expression *node = my_malloc(sizeof(Expression));
    node->kind = EXPR_MEMBER;
    node->lineno = lineno;
    node->left = structExpr;
    node->right = NULL;
    strncpy(node->memberName, member, sizeof(node->memberName) - 1);
    node->numArgs = 0;
    /* Type checking: ensure structExpr is of struct type and member exists in its definition */
    if (structExpr->exprType.base != BASE_STRUCT)
        type_error(inputfilename, lineno, "Member selection on non-struct type");
    // Lookup struct definition in symbol table:
    StructDef *sdef = structSymbols;
    bool found = false;
    while (sdef)
    {
        if (strcmp(sdef->name, structExpr->exprType.structName) == 0)
        {
            // Search for member in sdef
            for (int i = 0; i < sdef->numMembers; i++)
            {
                if (strcmp(sdef->members[i]->name, member) == 0)
                {
                    node->exprType = sdef->members[i]->declType;
                    /* Propagate const if the struct is const */
                    if (structExpr->exprType.isConst)
                        node->exprType.isConst = true;
                    found = true;
                    break;
                }
            }
            break;
        }
        sdef = sdef->next;
    }
    if (!found)
        type_error(inputfilename, lineno, "Member not found in struct");
    return node;
}

Expression *parse_assignment(lexer *L)
{
    Expression *left = parse_ternary(L);
    if (L->current.ID == TOKEN_EQUAL || L->current.ID == TOKEN_ADD_ASSIGN ||
        L->current.ID == TOKEN_SUB_ASSIGN ||
        L->current.ID == TOKEN_MUL_ASSIGN ||
        L->current.ID == TOKEN_DIV_ASSIGN)
    {
        Operator op;
        switch (L->current.ID)
        {
        case TOKEN_EQUAL:
            op = OP_ASSIGN;
            break;
        case TOKEN_ADD_ASSIGN:
            op = OP_PLUS_ASSIGN;
            break;
        case TOKEN_SUB_ASSIGN:
            op = OP_MINUS_ASSIGN;
            break;
        case TOKEN_MUL_ASSIGN:
            op = OP_MUL_ASSIGN;
            break;
        case TOKEN_DIV_ASSIGN:
            op = OP_DIV_ASSIGN;
            break;
        default:
            syntax_error(L, "assignment operator");
        }
        getNextToken(L);
        Expression *right = parse_assignment(L);
        left = make_assignment(left, right, L->lineno, op);
    }
    return left;
}

// Parse ternary operator: cond ? expr : expr
Expression *parse_ternary(lexer *L)
{
    Expression *cond = parse_logicalOr(L);
    if (L->current.ID == TOKEN_QUESTION)
    {
        getNextToken(L); // consume '?'
        Expression *trueExpr = parse_assignment(L);
        if (L->current.ID != TOKEN_COLON)
            syntax_error(L, "':' in ternary operator");
        getNextToken(L); // consume ':'
        Expression *falseExpr = parse_assignment(L);
        /* For now, we create a binary-like node to store ternary; type checking ensures both branches match */
        Expression *node = my_malloc(sizeof(Expression));
        node->kind = EXPR_TERNARY;
        node->lineno = cond->lineno;
        node->left = cond;  // condition
        node->right = NULL; // not used directly
        node->numArgs = 0;
        // For simplicity, we reuse args array to store two branches
        node->args = my_malloc(2 * sizeof(Expression *));
        node->args[0] = trueExpr;
        node->args[1] = falseExpr;
        /* Check that trueExpr and falseExpr have compatible types */
        if (!equal_types(&trueExpr->exprType, &falseExpr->exprType))
        {
            if (can_widen(&trueExpr->exprType, &falseExpr->exprType))
                widen_expression(trueExpr, &falseExpr->exprType);
            else if (can_widen(&falseExpr->exprType, &trueExpr->exprType))
                widen_expression(falseExpr, &trueExpr->exprType);
            else
                type_error(L->filename, L->lineno, "Incompatible types in ternary operator");
        }
        node->exprType = trueExpr->exprType;
        return node;
    }
    return cond;
}

// Parse logical OR: left || right
Expression *parse_logicalOr(lexer *L)
{
    Expression *node = parse_logicalAnd(L);
    while (L->current.ID == TOKEN_OR)
    {

        getNextToken(L);
        Expression *right = parse_logicalAnd(L);
        node = make_binary(node, OP_OR, right, L->lineno);
        /* Logical operators produce integer boolean (using int) */
        node->exprType.base = BASE_INT;
        node->exprType.isArray = false;
        node->exprType.isConst = false;
    }
    return node;
}

// Parse logical AND: left && right
Expression *parse_logicalAnd(lexer *L)
{
    Expression *node = parse_equality(L);
    while (L->current.ID == TOKEN_AMPERSAND && strcmp(L->current.attrb, "&&") == 0)
    {

        getNextToken(L);
        Expression *right = parse_equality(L);
        node = make_binary(node, OP_AND, right, L->lineno);
        node->exprType.base = BASE_INT;
        node->exprType.isArray = false;
        node->exprType.isConst = false;
    }
    return node;
}

// Parse equality operators: == and !=
Expression *parse_equality(lexer *L)
{
    Expression *node = parse_relational(L);
    while (L->current.ID == TOKEN_EQ || L->current.ID == TOKEN_NE)
    {

        Operator oper = (L->current.ID == TOKEN_EQ) ? OP_EQ : OP_NE;
        getNextToken(L);
        Expression *right = parse_relational(L);
        node = make_binary(node, oper, right, L->lineno);
        /* Result of equality is int */
        node->exprType.base = BASE_INT;
        node->exprType.isArray = false;
        node->exprType.isConst = false;
    }
    return node;
}

// Parse relational operators: <, <=, >, >=
Expression *parse_relational(lexer *L)
{
    Expression *node = parse_additive(L);
    while (L->current.ID == TOKEN_LESS || L->current.ID == TOKEN_LE ||
           L->current.ID == TOKEN_GREATER || L->current.ID == TOKEN_GE)
    {

        Operator oper;
        if (L->current.ID == TOKEN_LESS)
            oper = OP_LT;
        else if (L->current.ID == TOKEN_LE)
            oper = OP_LE;
        else if (L->current.ID == TOKEN_GREATER)
            oper = OP_GT;
        else
            oper = OP_GE;
        getNextToken(L);
        Expression *right = parse_additive(L);
        node = make_binary(node, oper, right, L->lineno);
        node->exprType.base = BASE_INT;
        node->exprType.isArray = false;
        node->exprType.isConst = false;
    }
    return node;
}

// Parse additive: left + right or left - right
Expression *parse_additive(lexer *L)
{
    Expression *node = parse_multiplicative(L);
    while (L->current.ID == TOKEN_PLUS || L->current.ID == TOKEN_MINUS)
    {

        Operator oper = (L->current.ID == TOKEN_PLUS) ? OP_PLUS : OP_MINUS;
        getNextToken(L);
        Expression *right = parse_multiplicative(L);
        node = make_binary(node, oper, right, L->lineno);
    }
    return node;
}

// Parse multiplicative: left * right or left / right or left % right
Expression *parse_multiplicative(lexer *L)
{
    Expression *node = parse_unary(L);
    while (L->current.ID == TOKEN_ASTERISK || L->current.ID == TOKEN_SLASH || L->current.ID == TOKEN_MOD)
    {

        Operator oper;
        if (L->current.ID == TOKEN_ASTERISK)
            oper = OP_MUL;
        else if (L->current.ID == TOKEN_SLASH)
            oper = OP_DIV;
        else
            oper = OP_MOD;
        getNextToken(L);
        Expression *right = parse_unary(L);
        node = make_binary(node, oper, right, L->lineno);
    }
    return node;
}

// Parse unary: '-' | '!' | cast | primary
Expression *parse_unary(lexer *L)
{
    if (L->current.ID == TOKEN_MINUS)
    {
        getNextToken(L);
        Expression *operand = parse_unary(L);
        return make_unary(OP_NEG, operand, L->lineno);
    }
    if (L->current.ID == TOKEN_EXCLAMATION)
    {
        getNextToken(L);
        Expression *operand = parse_unary(L);
        return make_unary(OP_NOT, operand, L->lineno);
    }
    if (L->current.ID == TOKEN_INC) // Handle ++
    {
        getNextToken(L);
        Expression *operand = parse_unary(L);
        return make_unary(OP_INC, operand, L->lineno);
    }
    if (L->current.ID == TOKEN_TILDE) // Handle ~
    {
        getNextToken(L);
        Expression *operand = parse_unary(L);
        return make_unary(OP_TILDE, operand, L->lineno);
    }
    if (L->current.ID == TOKEN_DEC) // Handle --
    {
        getNextToken(L);
        Expression *operand = parse_unary(L);
        return make_unary(OP_DEC, operand, L->lineno);
    }
    if (L->current.ID == TOKEN_LPAREN)
    {
        getNextToken(L);
        if (L->current.ID == TOKEN_IDENTIFIER || L->current.ID == TOKEN_TYPE)
        {
            char typeName[64];
            strncpy(typeName, L->current.attrb, sizeof(typeName) - 1);
            getNextToken(L);
            Type castType = {0};
            if (strcmp(typeName, "struct") == 0)
            {
                if (L->current.ID != TOKEN_IDENTIFIER)
                    syntax_error(L, "struct name");
                castType.base = BASE_STRUCT;
                strncpy(castType.structName, L->current.attrb, sizeof(castType.structName) - 1);
                getNextToken(L);
            }
            else if (strcmp(typeName, "int") == 0)
            {
                castType.base = BASE_INT;
            }
            else if (strcmp(typeName, "float") == 0)
            {
                castType.base = BASE_FLOAT;
            }
            else if (strcmp(typeName, "char") == 0)
            {
                castType.base = BASE_CHAR;
            }
            else
            {
                syntax_error(L, "valid type in cast");
            }
            castType.isConst = false;
            castType.isArray = false;
            if (L->current.ID != TOKEN_RPAREN)
                syntax_error(L, "')' after cast type");
            getNextToken(L);
            Expression *expr = parse_unary(L);
            return make_cast(castType, expr, L->lineno);
        }
        else
        {
            Expression *expr = parse_assignment(L);
            if (L->current.ID != TOKEN_RPAREN)
                syntax_error(L, "')'");
            getNextToken(L);
            return expr;
        }
    }
    return parse_primary(L);
}

// Parse primary expressions: literals, identifiers, parenthesized expr, array indexing, function calls, and member selection.
Expression *parse_primary(lexer *L)
{

    Expression *node = NULL;
    if (L->current.ID == TOKEN_LPAREN)
    {
        getNextToken(L); // consume '('
        node = parse_assignment(L);
        if (L->current.ID != TOKEN_RPAREN)
            syntax_error(L, "')'");
        getNextToken(L);
    }
    else if (L->current.ID == TOKEN_INT || L->current.ID == TOKEN_REAL ||
             L->current.ID == TOKEN_CHAR || L->current.ID == TOKEN_STRING)
    {
        node = make_literal(L->current.attrb,
                            (L->current.ID == TOKEN_INT) ? (Type){BASE_INT, false, false, ""} : (L->current.ID == TOKEN_REAL) ? (Type){BASE_FLOAT, false, false, ""}
                                                                                            : (L->current.ID == TOKEN_CHAR)   ? (Type){BASE_CHAR, false, false, ""}
                                                                                                                              : (Type){BASE_CHAR, true, true, ""},
                            L->lineno);
        getNextToken(L);
    }
    else if (L->current.ID == TOKEN_IDENTIFIER)
    {
        /* Parse an identifier. This might be a variable, function call, or struct member access */

        node = make_identifier(L->current.attrb, L->lineno);
        getNextToken(L);

        /* Handle array indexing: identifier followed by '[' expression ']' */
        while (L->current.ID == TOKEN_LBRACKET)
        {
            getNextToken(L); // consume '['
            Expression *indexExpr = parse_assignment(L);
            if (L->current.ID != TOKEN_RBRACKET)
                syntax_error(L, "']'");
            getNextToken(L); // consume ']'
            node = make_index(node, indexExpr, L->lineno);
        }
        /* Handle function call: identifier followed by '(' ... ')' */
        if (L->current.ID == TOKEN_LPAREN)
        {
            getNextToken(L); // consume '('
            // Parse argument list (comma separated)
            int numArgs = 0;
            Expression **args = NULL;
            if (L->current.ID != TOKEN_RPAREN)
            {
                args = my_malloc(10 * sizeof(Expression *)); // support up to 10 args for simplicity
                while (1)
                {
                    args[numArgs++] = parse_assignment(L);
                    if (L->current.ID == TOKEN_COMMA)
                    {
                        getNextToken(L);
                    }
                    else
                    {
                        break;
                    }
                }
            }
            if (L->current.ID != TOKEN_RPAREN)
                syntax_error(L, "')' after function call");
            getNextToken(L); // consume ')'
            node = make_call(node, args, numArgs, L->lineno);
        }
        // Handle member selection: identifier '.' identifier
        while (L->current.ID == TOKEN_DOT)
        {
            getNextToken(L); // consume '.'
            if (L->current.ID != TOKEN_IDENTIFIER)
                syntax_error(L, "member name after '.'");
            char member[64];
            strncpy(member, L->current.attrb, sizeof(member) - 1);
            getNextToken(L);
            node = make_member(node, member, L->lineno);
        }
        if (L->current.ID == TOKEN_INC || L->current.ID == TOKEN_DEC)
        {
            Operator op = (L->current.ID == TOKEN_INC) ? OP_INC : OP_DEC;
            getNextToken(L);
            node = make_unary(op, node, L->lineno);
        }
    }
    else
    {
        syntax_error(L, "primary expression");
    }
    return node;
}

Statement *parser_declaration(lexer *L, LookaheadBuffer *buf, bool isGlobal, bool isConst)
{
    int buf_pos = 0;
    Statement *stmt = my_malloc(sizeof(Statement));
    stmt->kind = STMT_DECL;
    stmt->lineno = L->lineno;

    Declaration *decl = &stmt->u.decl;
    memset(decl, 0, sizeof(Declaration));

    // Optional const modifier
    token t = (buf_pos < buf->count) ? buf->tokens[buf_pos++] : L->current;
    
    // Parse type
    Type type = {0};
    if (t.ID == TOKEN_TYPE)
    {
        if (strcmp(t.attrb, "int") == 0)
        {
            type.base = BASE_INT;
        }
        else if (strcmp(t.attrb, "float") == 0)
        {
            type.base = BASE_FLOAT;
        }
        else if (strcmp(t.attrb, "char") == 0)
        {
            type.base = BASE_CHAR;
        }
        else if (strcmp(t.attrb, "void") == 0)
        {
            type_error(L->filename, L->lineno, "Variable cannot be declared void");
        }
        else if (strcmp(t.attrb, "struct") == 0)
        {
            type.base = BASE_STRUCT;
            if (buf_pos <= buf->count)
                getNextToken(L);
            t = (buf_pos < buf->count) ? buf->tokens[buf_pos++] : L->current;
            if (t.ID != TOKEN_IDENTIFIER)
                syntax_error(L, "struct name");
            strncpy(type.structName, t.attrb, sizeof(type.structName) - 1);
        }
        else
        {
            syntax_error(L, "type specifier");
        }
    }
    else
    {
        syntax_error(L, "type specifier");
    }
    type.isConst = isConst;
    type.isArray = false;
    t = (buf_pos < buf->count) ? buf->tokens[buf_pos++] : L->current;
    if (t.ID != TOKEN_IDENTIFIER)
        syntax_error(L, "variable name");
    strncpy(decl->name, t.attrb, sizeof(decl->name) - 1);
    // Check for array declaration
    if (L->current.ID == TOKEN_LBRACKET)
    {
        getNextToken(L); // consume '['
        while(L->current.ID != TOKEN_RBRACKET && L->current.ID != END)
            getNextToken(L);

        if (L->current.ID != TOKEN_RBRACKET)
            syntax_error(L, "']'");
        getNextToken(L); // consume ']'
        type.isArray = true;
    }
    decl->declType = type;

    // Add declaration to symbol table
    add_variable(decl->name, type, isGlobal);
    // Optional initialization
    if (L->current.ID == TOKEN_EQUAL)
    {
        getNextToken(L); // consume '='
        decl->init = parse_assignment(L);
        decl->initialized = true;
        if (!equal_types(&decl->declType, &decl->init->exprType))
        {
            if (can_widen(&decl->init->exprType, &decl->declType))
            {
                widen_expression(decl->init, &decl->declType);
            }
            else
            {
                type_error(L->filename, L->lineno, "Initializer type does not match declared type");
            }
        }
    }
    else
    {
        decl->initialized = false;
    }

    return stmt;
}

// A statement can be a declaration, an expression statement, or a return statement, etc.
Statement *parser_statement(lexer *L, LookaheadBuffer *buf, bool isGlobal, bool isConst)
{
    Statement *stmt = NULL;

    if (buf && buf->count > 0)
    {
        // Check if this is a declaration
        token t = buf->tokens[0];
        if (t.ID == TOKEN_TYPE)
        {
            if (L->current.ID != TOKEN_LBRACKET && L->current.ID != TOKEN_SEMICOLON)
                getNextToken(L);
            stmt = parser_declaration(L, buf, isGlobal, isConst);
            return stmt;
        }
    }
    // Handle return statement
    if (L->current.ID == TOKEN_RETURN)
    {
        stmt = my_malloc(sizeof(Statement));
        stmt->kind = STMT_RETURN;
        stmt->lineno = L->lineno;
        getNextToken(L); // consume 'return'
        Expression *retExpr = NULL;
        if (L->current.ID != TOKEN_SEMICOLON)
        {
            retExpr = parse_assignment(L);
        }
        stmt->u.expr = retExpr;
        if (L->current.ID != TOKEN_SEMICOLON)
            syntax_error(L, "';'");
        getNextToken(L);
        return stmt;
    }

    // Handle compound statement
    if (L->current.ID == TOKEN_LBRACE)
    {
        return parse_compound(L); // parse_compound uses lexer directly
    }

    // Otherwise, parse as an expression statement
    stmt = my_malloc(sizeof(Statement));
    stmt->kind = STMT_EXPR;
    stmt->lineno = L->lineno;
    stmt->u.expr = parse_assignment(L);
    return stmt;
}

// Parse a compound statement: '{' { statement } '}'
Statement *parse_compound(lexer *L)
{
    if (L->current.ID != TOKEN_LBRACE)
        syntax_error(L, "'{'");
    getNextToken(L);
    Statement **stmts = my_malloc(100 * sizeof(Statement *));
    int count = 0;
    LookaheadBuffer buf;
    init_lookahead(&buf);
    bool isConst;
    while (L->current.ID != TOKEN_RBRACE)
    {
        clear_lookahead(&buf);
        isConst = false;
        if (L->current.ID == TOKEN_CONST) {
            isConst = true;
            getNextToken(L);
        }
        if (L->current.ID == TOKEN_TYPE)
        {
            push_token(&buf, L->current);
            getNextToken(L);
            if (L->current.ID == TOKEN_CONST) {
                if (isConst)
                    syntax_error(L,"type");
                isConst = true;
                getNextToken(L);
            }
            if (L->current.ID == TOKEN_IDENTIFIER)
            {
                push_token(&buf, L->current);
                getNextToken(L);
            }
        }
        if (L->current.ID == TOKEN_IDENTIFIER)
            {
            stmts[count++] = parser_statement(L, NULL, true,isConst);
            }
            else
        {
            do
            {
                if (L->current.ID != TOKEN_LBRACKET && L->current.ID != TOKEN_SEMICOLON)
                {
                    if (buf.tokens[0].ID == TOKEN_TYPE && buf.tokens[1].ID == TOKEN_IDENTIFIER)
                        stmts[count++] = parser_statement(L, &buf, false, isConst);
                    if (L->current.ID == TOKEN_IDENTIFIER)
                        buf.tokens[1] = L->current;
                }
                stmts[count++] = parser_statement(L, &buf, false, isConst);


            } while (L->current.ID == TOKEN_COMMA);
        }
        if (L->current.ID != TOKEN_SEMICOLON && L->current.ID != TOKEN_RBRACE)
        {
            syntax_error(L, "';'");
        }

        if (L->current.ID == TOKEN_SEMICOLON)
            getNextToken(L);
    }
    if (L->current.ID != TOKEN_RBRACE)
        syntax_error(L, "'}'");
    getNextToken(L); // consume '}'
    Statement *compound = my_malloc(sizeof(Statement));
    compound->kind = STMT_COMPOUND;
    compound->lineno = L->lineno;
    compound->numStmts = count;
    compound->stmts = stmts;
    return compound;
}

Statement *parse_function_declaration(lexer *L, LookaheadBuffer *buf)
{
    int buf_pos = 0;

    bool isConst = false;
    token t = (buf_pos < buf->count) ? buf->tokens[buf_pos++] : L->current;
    if (t.ID == TOKEN_IDENTIFIER && strcmp(t.attrb, "const") == 0)
    {
        isConst = true;
        if (buf_pos <= buf->count)
            getNextToken(L);
        t = (buf_pos < buf->count) ? buf->tokens[buf_pos++] : L->current;
    }

    Type retType = {0};
    if (t.ID == TOKEN_TYPE)
    {
        if (strcmp(t.attrb, "int") == 0)
            retType.base = BASE_INT;
        else if (strcmp(t.attrb, "float") == 0)
            retType.base = BASE_FLOAT;
        else if (strcmp(t.attrb, "char") == 0)
            retType.base = BASE_CHAR;
        else if (strcmp(t.attrb, "void") == 0)
            retType.base = BASE_VOID;
        else if (strcmp(t.attrb, "struct") == 0)
        {
            retType.base = BASE_STRUCT;
            if (buf_pos <= buf->count)
                getNextToken(L);
            t = (buf_pos < buf->count) ? buf->tokens[buf_pos++] : L->current;
            if (t.ID != TOKEN_IDENTIFIER)
                syntax_error(L, "struct name");
            strncpy(retType.structName, t.attrb, sizeof(retType.structName) - 1);
        }
        else
        {
            syntax_error(L, "valid return type");
        }
    }
    else
    {
        syntax_error(L, "return type");
    }
    retType.isConst = isConst;
    retType.isArray = false;

    // Parse function name
    t = (buf_pos < buf->count) ? buf->tokens[buf_pos++] : L->current;
    if (t.ID != TOKEN_IDENTIFIER)
        syntax_error(L, "function name");
    char funcName[64];
    strncpy(funcName, t.attrb, sizeof(funcName) - 1);
    if (buf_pos <= buf->count)
        getNextToken(L);

    t = (buf_pos < buf->count) ? buf->tokens[buf_pos++] : L->current;
    if (t.ID != TOKEN_LPAREN)
        syntax_error(L, "'(' after function name");

    // Parse parameter list
    Declaration **params = my_malloc(20 * sizeof(Declaration *));
    int numParams = 0;
    LookaheadBuffer buff;
    init_lookahead(&buff);
    while (L->current.ID != TOKEN_RPAREN)
    {
        Declaration *param = my_malloc(sizeof(Declaration));

        clear_lookahead(&buff);
        push_token(&buff, L->current);

        bool pConst = false;
        if (L->current.ID == TOKEN_IDENTIFIER && strcmp(L->current.attrb, "const") == 0)
        {
            pConst = true;
            getNextToken(L);
        }
        Type pType = {0};
        if (buff.count > 0 && buff.tokens[0].ID != TOKEN_TYPE)
            syntax_error(L, "parameter type");
        if (strcmp(buff.tokens[0].attrb, "int") == 0)
            pType.base = BASE_INT;
        else if (strcmp(buff.tokens[0].attrb, "float") == 0)
            pType.base = BASE_FLOAT;
        else if (strcmp(buff.tokens[0].attrb, "char") == 0)
            pType.base = BASE_CHAR;
        else if (strcmp(buff.tokens[0].attrb, "void") == 0)
            type_error(L->filename, L->lineno, "Parameter type cannot be void");
        else if (strcmp(buff.tokens[0].attrb, "struct") == 0)
        {
            pType.base = BASE_STRUCT;
            getNextToken(L);
            if (L->current.ID != TOKEN_IDENTIFIER)
                syntax_error(L, "struct name in parameter");
            strncpy(pType.structName, L->current.attrb, sizeof(pType.structName) - 1);
        }
        else
            syntax_error(L, "valid parameter type");
        pType.isConst = pConst;
        pType.isArray = false;
        getNextToken(L); // consume type name
        push_token(&buff, L->current);

        if (L->current.ID != TOKEN_IDENTIFIER)
            syntax_error(L, "parameter name");
        strncpy(param->name, L->current.attrb, sizeof(param->name) - 1);
        getNextToken(L);
        param->declType = pType;
        add_variable(param->name, param->declType, false);

        param->initialized = false;
        param->init = NULL;
        params[numParams++] = param;

        if (L->current.ID == TOKEN_COMMA)
        {
            getNextToken(L);
            continue;
        }
        else
            break;
    }

    if (L->current.ID != TOKEN_RPAREN)
        syntax_error(L, "')'");
    getNextToken(L);

    // Create or update function symbol
    Function *func = my_malloc(sizeof(Function));
    strncpy(func->name, funcName, sizeof(func->name) - 1);
    func->returnType = retType;
    func->numParams = numParams;
    func->params = params;
    func->defined = false;
    func->body = NULL;
    func->next = funcSymbols;
    funcSymbols = func;

    Statement *stmt = my_malloc(sizeof(Statement));
    stmt->lineno = L->lineno;
    if (L->current.ID == TOKEN_SEMICOLON)
    {
        stmt->kind = STMT_EXPR;
        stmt->u.expr = NULL;
        getNextToken(L);
    }
    else
    {
        stmt->kind = STMT_COMPOUND;
        stmt->u.expr = NULL;
        Statement *body = parse_compound(L);
        func->body = body;
        func->defined = true;
        stmt = body;
        stmt->u.func = func;
    }
    return stmt;
}


Statement *parse_struct(lexer *L, LookaheadBuffer *buf)
{
    int buf_pos = 0;

    token t = (buf_pos < buf->count) ? buf->tokens[buf_pos++] : L->current;
    if (t.ID != TOKEN_IDENTIFIER || strcmp(t.attrb, "struct") != 0)
        syntax_error(L, "struct");
    if (buf_pos <= buf->count)
        getNextToken(L);

    t = (buf_pos < buf->count) ? buf->tokens[buf_pos++] : L->current;
    if (t.ID != TOKEN_IDENTIFIER)
        syntax_error(L, "struct name");
    char structName[64];
    strncpy(structName, t.attrb, sizeof(structName) - 1);
    if (buf_pos <= buf->count)
        getNextToken(L);

   
    t = (buf_pos < buf->count) ? buf->tokens[buf_pos++] : L->current;
    if (t.ID != TOKEN_LBRACE)
        syntax_error(L, "'{' after struct name");
    if (buf_pos <= buf->count)
        getNextToken(L); 
    StructDef *sdef = my_malloc(sizeof(StructDef));
    strncpy(sdef->name, structName, sizeof(sdef->name) - 1);
    sdef->numMembers = 0;
    sdef->members = my_malloc(20 * sizeof(Declaration *));
    sdef->isGlobal = true;

    while (L->current.ID != TOKEN_RBRACE)
    {
        Statement *memberStmt = parser_statement(L, buf, true, false); // Note: parser_statement uses lexer directly
        sdef->members[sdef->numMembers] = my_malloc(sizeof(Declaration));
        *(sdef->members[sdef->numMembers]) = memberStmt->u.decl;
        sdef->numMembers++;
    }
    if (L->current.ID != TOKEN_RBRACE)
        syntax_error(L, "'}' after struct definition");
    getNextToken(L); 

    if (L->current.ID != TOKEN_SEMICOLON)
        syntax_error(L, "';' after struct definition");
    getNextToken(L);

    sdef->next = structSymbols;
    structSymbols = sdef;

    Statement *dummy = my_malloc(sizeof(Statement));
    dummy->kind = STMT_EXPR;
    dummy->lineno = L->lineno;
    dummy->u.expr = NULL;
    return dummy;
}

// Main parsing function
Statement **parse_program(lexer *L, int *stmtCount)
{
    inputfilename = L->filename;
    Statement **stmts = my_malloc(200 * sizeof(Statement *));
    int count = 0;
    LookaheadBuffer buf;
    init_lookahead(&buf);
    bool isConst;
    while (L->current.ID != END)
    {

        clear_lookahead(&buf);
        isConst = false;
        if(L->current.ID == TOKEN_CONST)
        {
            isConst = true;
            getNextToken(L);
        }
        push_token(&buf, L->current); // Store first token (e.g., "int", "struct")
        if (buf.count > 0 && (buf.tokens[0].ID == TOKEN_TYPE ||
                              (buf.tokens[0].ID == TOKEN_IDENTIFIER && strcmp(buf.tokens[0].attrb, "struct") == 0)))
        {
            getNextToken(L);
            if(L->current.ID == TOKEN_CONST)
            {
                isConst = true;
                getNextToken(L);
            }
            // Line is either a function, variable or struct
            push_token(&buf, L->current);

            if (L->current.ID == TOKEN_IDENTIFIER)
            {
                getNextToken(L);

                push_token(&buf, L->current);

                // Check if struct: struct identifier {
                if (buf.tokens[0].ID == TOKEN_IDENTIFIER && strcmp(buf.tokens[0].attrb, "struct") == 0 &&
                    buf.tokens[1].ID == TOKEN_IDENTIFIER && buf.tokens[2].ID == TOKEN_LBRACE)
                {
                    stmts[count++] = parse_struct(L, &buf);
                }

                // Check if function: type identifier (
                else if (buf.tokens[0].ID == TOKEN_TYPE && buf.tokens[1].ID == TOKEN_IDENTIFIER && buf.tokens[2].ID == TOKEN_LPAREN)
                {
                    stmts[count++] = parse_function_declaration(L, &buf);
                }
                // Otherwise, assume variable declaration
                else
                {
                    do
                    {
                        if (L->current.ID != TOKEN_LBRACKET && L->current.ID != TOKEN_SEMICOLON)
                        {

                            getNextToken(L);
                            if (L->current.ID == TOKEN_IDENTIFIER)
                            {

                                buf.tokens[1] = L->current;
                            }
                        }
                        stmts[count++] = parser_statement(L, &buf, false, isConst);

                    } while (L->current.ID == TOKEN_COMMA);

                    if (L->current.ID != TOKEN_SEMICOLON)
                    {
                        syntax_error(L, "';'");
                    }

                    getNextToken(L);
                }
            }
            else if (buf.tokens[0].ID == TOKEN_IDENTIFIER && strcmp(buf.tokens[0].attrb, "struct") == 0)
            {
                // Struct definition with possible missing identifier (e.g., struct { ... })
                stmts[count++] = parse_struct(L, &buf);
            }
            else
            {
                syntax_error(L, "identifier after type");
            }
        }
        else
        {
            // Other statements (e.g., expressions)
            stmts[count++] = parser_statement(L, NULL, true, isConst);
        }
    }

    *stmtCount = count;
    return stmts;
}

void type_check_expression(Expression *expr, const char *filename, FILE *output)
{
    if (!expr)
        return;
    switch (expr->kind)
    {
    case EXPR_LITERAL:
        // Type set in parse_primary
        break;
    case EXPR_IDENTIFIER:
        {
            VarSymbol *vs = lookup_variable(expr->value);
            if (vs)
            {
                expr->exprType = vs->type;
            }
            else
            {
                Function *func = lookup_function(expr->value);
                if (!func)
                {
                    char msg[256];
                    snprintf(msg, sizeof(msg), "Undeclared variable or function %s", expr->value);
                    type_error(filename, expr->lineno, msg);
                }
                expr->exprType = func->returnType;
            }
        }
        break;
    case EXPR_UNARY:
        {
            type_check_expression(expr->right, filename, output);
            Type operandType = expr->right->exprType;
            if (expr->op == OP_NEG)
            {
                if (operandType.base != BASE_INT && operandType.base != BASE_FLOAT && operandType.base != BASE_CHAR)
                {
                    char typeStr[128];
                    format_type(&operandType, typeStr, sizeof(typeStr));
                    char msg[256];
                    snprintf(msg, sizeof(msg), "Invalid operation: - %s", typeStr);
                    type_error(filename, expr->lineno, msg);
                }
                expr->exprType = operandType;
            }
            else if (expr->op == OP_TILDE)
            {
                if (operandType.base != BASE_INT && operandType.base != BASE_CHAR)
                {
                    char typeStr[128];
                    format_type(&operandType, typeStr, sizeof(typeStr));
                    char msg[256];
                    snprintf(msg, sizeof(msg), "Invalid operation: ~ %s", typeStr);
                    type_error(filename, expr->lineno, msg);
                }
                expr->exprType = operandType;
            }
            else if (expr->op == OP_NOT)
            {
                if (operandType.base == BASE_VOID)
                {
                    char typeStr[128];
                    format_type(&operandType, typeStr, sizeof(typeStr));
                    char msg[256];
                    snprintf(msg, sizeof(msg), "Invalid operation: ! %s", typeStr);
                    type_error(filename, expr->lineno, msg);
                }
                expr->exprType = (Type){BASE_INT, false, false, ""}; // ! returns int
            }
            else if (expr->op == OP_INC || expr->op == OP_DEC)
            {
                if (operandType.isConst)
                {
                    char typeStr[128];
                    format_type(&operandType, typeStr, sizeof(typeStr));
                    char msg[256];
                    snprintf(msg, sizeof(msg), "Invalid operation: %s %s",
                             expr->op == OP_INC ? "++" : "--", typeStr);
                    type_error(filename, expr->lineno, msg);
                }
                if (operandType.base != BASE_INT && operandType.base != BASE_CHAR)
                {
                    char typeStr[128];
                    format_type(&operandType, typeStr, sizeof(typeStr));
                    char msg[256];
                    snprintf(msg, sizeof(msg), "Invalid operation: %s %s",
                             expr->op == OP_INC ? "++" : "--", typeStr);
                    type_error(filename, expr->lineno, msg);
                }
                expr->exprType = operandType;
            }
        }
        break;
    case EXPR_BINARY:
        {
            type_check_expression(expr->left, filename, output);
            type_check_expression(expr->right, filename, output);
            Type leftType = expr->left->exprType;
            Type rightType = expr->right->exprType;
            if (expr->op == OP_PLUS || expr->op == OP_MINUS ||
                expr->op == OP_MUL || expr->op == OP_DIV || expr->op == OP_MOD)
            {
                if ((leftType.base != BASE_INT && leftType.base != BASE_FLOAT) ||
                    (rightType.base != BASE_INT && rightType.base != BASE_FLOAT))
                {
                    char leftStr[52], rightStr[52];
                    format_type(&leftType, leftStr, sizeof(leftStr));
                    format_type(&rightType, rightStr, sizeof(rightStr));
                    char msg[256];
                    snprintf(msg, sizeof(msg), "Invalid operation: %s %s %s",
                             leftStr, expr->op == OP_PLUS ? "+" : expr->op == OP_MINUS ? "-" :
                             expr->op == OP_MUL ? "*" : expr->op == OP_DIV ? "/" : "%", rightStr);
                    type_error(filename, expr->lineno, msg);
                }
                if (can_widen(&leftType, &rightType))
                {
                    widen_expression(expr->left, &rightType);
                    expr->exprType = rightType;
                }
                else if (can_widen(&rightType, &leftType))
                {
                    widen_expression(expr->right, &leftType);
                    expr->exprType = leftType;
                }
                else
                {
                    expr->exprType = leftType;
                }
            }
            else if (expr->op == OP_EQ || expr->op == OP_NE ||
                     expr->op == OP_LT || expr->op == OP_LE ||
                     expr->op == OP_GT || expr->op == OP_GE)
            {
                expr->exprType = (Type){BASE_INT, false, false, ""};
            }
            else if (expr->op == OP_AND || expr->op == OP_OR)
            {
                expr->exprType = (Type){BASE_INT, false, false, ""};
            }
        }
        break;
    case EXPR_ASSIGN:
        {
            type_check_expression(expr->left, filename, output);
            type_check_expression(expr->right, filename, output);
            Type leftType = expr->left->exprType;
            Type rightType = expr->right->exprType;
            if (!equal_types(&leftType, &rightType))
            {
                if (can_widen(&rightType, &leftType))
                {
                    widen_expression(expr->right, &leftType);
                }
                else
                {
                    char leftStr[52], rightStr[52];
                    format_type(&leftType, leftStr, sizeof(leftStr));
                    format_type(&rightType, rightStr, sizeof(rightStr));
                    char msg[256];
                    snprintf(msg, sizeof(msg), "Type mismatch: %s = %s", leftStr, rightStr);
                    type_error(filename, expr->lineno, msg);
                }
            }
            expr->exprType = leftType;
        }
        break;
    case EXPR_CALL:
        {
            type_check_expression(expr->left, filename, output);
            for (int i = 0; i < expr->numArgs; i++)
                type_check_expression(expr->args[i], filename, output);
            expr->exprType = expr->left->exprType;
        }
        break;
    case EXPR_INDEX:
        {
            type_check_expression(expr->left, filename, output);
            type_check_expression(expr->right, filename, output);
            expr->exprType = expr->left->exprType;
            expr->exprType.isArray = false;
        }
        break;
    case EXPR_MEMBER:
        {
            type_check_expression(expr->left, filename, output);
            // expr->exprType set in make_member
        }
        break;
    case EXPR_TERNARY:
        {
            type_check_expression(expr->left, filename, output);
            type_check_expression(expr->args[0], filename, output);
            type_check_expression(expr->args[1], filename, output);
            expr->exprType = expr->args[0]->exprType;
        }
        break;
    case EXPR_CAST:
        {
            type_check_expression(expr->left, filename, output);
            // expr->exprType set in make_cast
        }
        break;
    }
}

void type_check_statement(Statement *stmt, const char *filename, FILE *output)
{
    if (!stmt)
        return;
    static Function *func = NULL;
    if (stmt->kind == STMT_COMPOUND && stmt->u.func)
        func = stmt->u.func;
    switch (stmt->kind)
    {
    case STMT_EXPR:
    if (stmt->u.expr)
    {
        type_check_expression(stmt->u.expr, filename, output);
        char typeStr[128];
        format_type(&stmt->u.expr->exprType, typeStr, sizeof(typeStr));
        fprintf(output, "File %s Line %d: expression has type %s\n", filename, stmt->lineno, typeStr);
    }
    break;
    case STMT_DECL:
    {
        char typeStr[128];
        format_type(&stmt->u.decl.declType, typeStr, sizeof(typeStr));
        if (stmt->u.decl.initialized && stmt->u.decl.init)
        {
            type_check_expression(stmt->u.decl.init, filename, output);
        }
        break;
    }
    case STMT_RETURN:
        if (!func)
        {
            fprintf(stderr, "No function context for return statement\n");
            exit(1);
        }
        Type expectedType = func->returnType;
        
        Type actualType = stmt->u.expr ? stmt->u.expr->exprType : (Type){BASE_VOID, false, false, ""};
        if (!equal_types(&actualType, &expectedType))
        {
            char actualStr[52], expectedStr[52];
            format_type(&actualType, actualStr, sizeof(actualStr));
            format_type(&expectedType, expectedStr, sizeof(expectedStr));
            char msg[256];
            snprintf(msg, sizeof(msg), "Return type mismatch: was %s, expected %s", actualStr, expectedStr);
            type_error(filename, stmt->lineno, msg);
        }
        if (stmt->u.expr)
            type_check_expression(stmt->u.expr, filename, output);
        break;
    case STMT_COMPOUND:
        for (int i = 0; i < stmt->numStmts; i++)
            type_check_statement(stmt->stmts[i], filename, output);
        break;
    }
}
