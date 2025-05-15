#include "parser_ast.h"
#include "lexer.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

typedef struct StructTable
{
    char *key;
    char *value;
} StructTable;

// Symbol table definitions
VarSymbol *varSymbols = NULL;
Function *funcSymbols = NULL;
StructDef *structSymbols = NULL;
StructTable *struct_table = NULL;
int size = 0;
int capacity = 0;

bool is_numeric_type(const Type *type)
{
    return type->base == BASE_CHAR || type->base == BASE_INT || type->base == BASE_FLOAT;
}

void append(StructTable **table, int *size, int *capacity, const char *key, const char *value)
{
    if (*size >= *capacity)
    {
        *capacity = (*capacity == 0) ? 1 : *capacity * 2;
        StructTable *temp = realloc(*table, *capacity * sizeof(StructTable));
        if (temp == NULL)
        {
            printf("Memory allocation failed!\n");
            exit(1);
        }
        *table = temp;
    }

    // Allocate memory for key and value
    (*table)[*size].key = strdup(key);     // Copy key
    (*table)[*size].value = strdup(value); // Copy value
    if ((*table)[*size].key == NULL || (*table)[*size].value == NULL)
    {
        printf("String duplication failed!\n");
        exit(1);
    }

    (*size)++;
}

char *lookup(StructTable *table, int size, const char *key)
{
    for (int i = 0; i < size; i++)
    {
        if (strcmp(table[i].key, key) == 0)
        {
            return table[i].value;
        }
    }
    return NULL; // Key not found
}

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
    if (from->base == BASE_INT && to->base == BASE_CHAR)
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

// Syntax error for parser
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

void add_variable(lexer *L, const char *name, Type type, bool isGlobal, Function *currentFunc)
{
    if (!isGlobal && currentFunc)
    {
        VarSymbol *local_vs = currentFunc->locals;
        local_vs = currentFunc->locals;
        while (local_vs)
        {
            if (strcmp(local_vs->name, name) == 0)
            {
                fprintf(stderr, "Error: Duplicate local variable '%s' in function '%s' at line %d\n",
                        name, currentFunc->name, L->lineno);
                exit(1);
            }
            local_vs = local_vs->next;
        }
    }
    else if (isGlobal)
    {
        VarSymbol *global_vs = varSymbols;
        while (global_vs)
        {
            if (strcmp(global_vs->name, name) == 0)
            {
                fprintf(stderr, "Error: Duplicate global variable '%s' at line %d\n",
                        name, L->lineno);
                exit(1);
            }
            global_vs = global_vs->next;
        }
    }

    VarSymbol *vs = my_malloc(sizeof(VarSymbol));
    strncpy(vs->name, name, sizeof(vs->name) - 1);
    vs->name[sizeof(vs->name) - 1] = '\0';
    vs->type = type;
    vs->isGlobal = isGlobal;
    vs->localIndex = -1;
    vs->next = NULL;
    vs->type.lineDeclared = L->lineno;

    if (isGlobal)
    {
        vs->next = varSymbols;
        varSymbols = vs;
    }
    else if (currentFunc)
    {
        vs->next = currentFunc->locals;
        currentFunc->locals = vs;
    }
    else
    {
        fprintf(stderr, "Error: Local variable '%s' declared without function context\n", name);
        exit(1);
    }
}
void add_struct(const char *name, bool isGlobal)
{
    StructDef *struc = my_malloc(sizeof(StructDef));
    strncpy(struc->name, name, sizeof(struc->name) - 1);
    struc->isGlobal = isGlobal;
    struc->next = structSymbols;
    structSymbols = struc;
}

void add_function(const char *name, Type returnType, int numParams, VarSymbol **params, bool defined)
{
    Function *f = my_malloc(sizeof(Function));
    strncpy(f->name, name, sizeof(f->name) - 1);
    f->name[sizeof(f->name) - 1] = '\0';
    f->returnType = returnType;
    f->numParams = numParams;
    VarSymbol **new_params = NULL;
    if (numParams > 0)
    {
        new_params = my_malloc(numParams * sizeof(VarSymbol *));
        for (int i = 0; i < numParams; i++)
        {
            new_params[i] = my_malloc(sizeof(VarSymbol));
            *new_params[i] = *params[i]; // Copy Declaration struct
        }
    }
    f->params = new_params;
    f->body = NULL;
    f->defined = defined;
    f->stackLimit = 0;
    f->next = funcSymbols;
    funcSymbols = f;
}

VarSymbol *lookup_variable(const char *name, Function *currentFunc)
{
    if (currentFunc)
    {
        VarSymbol *vs = currentFunc->locals;
        while (vs)
        {
            if (strcmp(vs->name, name) == 0)
            {
                return vs;
            }
            vs = vs->next;
        }
    }
    VarSymbol *vs = varSymbols;
    while (vs)
    {
        if (strcmp(vs->name, name) == 0 && vs->isGlobal)
        {
            return vs;
        }
        vs = vs->next;
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
Expression *make_identifier(const char *name, int lineno, Function *currentFunc)
{
    Expression *node = my_malloc(sizeof(Expression));
    node->kind = EXPR_IDENTIFIER;
    node->lineno = lineno;
    strncpy(node->value, name, sizeof(node->value) - 1);

    // Check if it's a variable
    VarSymbol *vs = lookup_variable(name, currentFunc);
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
            StructDef *struc = lookup_struct(name);
            if (!struc)
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
                     leftType, op == OP_EQ ? "==" : op == OP_NE ? "!="
                                                : op == OP_LT   ? "<"
                                                : op == OP_LE   ? "<="
                                                : op == OP_GT   ? ">"
                                                : op == OP_GE   ? ">="
                                                : op == OP_AND  ? "&&"
                                                                : "||",
                     rightType);
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
    // Check type compatibility (allow automatic widening)
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
        format_type(&func->params[i]->type, paramType, sizeof(paramType));
        if (!equal_types(&args[i]->exprType, &func->params[i]->type))
        {
            if (!can_widen(&args[i]->exprType, &func->params[i]->type))
            {
                char msg[256];
                snprintf(msg, sizeof(msg), "In call to %s: Parameter #%d should be %s, was %s",
                         func->name, i + 1, paramType, argType);
                type_error(inputfilename, lineno, msg);
            }
            widen_expression(args[i], &func->params[i]->type);
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
        if (strcmp(sdef->name, lookup(struct_table, size, structExpr->exprType.structName)) == 0)
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

Expression *parse_assignment(lexer *L, Function *currentFunc)
{
    Expression *left = parse_ternary(L, currentFunc);
    if (L->current.ID == TOKEN_EQUAL || L->current.ID == TOKEN_ADD_ASSIGN ||
        L->current.ID == TOKEN_SUB_ASSIGN ||
        L->current.ID == TOKEN_MUL_ASSIGN ||
        L->current.ID == TOKEN_DIV_ASSIGN)
    {
        Operator op;
        Operator bin_op = OP_ASSIGN;
        bool is_compound = false;
        switch (L->current.ID)
        {
        case TOKEN_EQUAL:
            op = OP_ASSIGN;
            break;
        case TOKEN_ADD_ASSIGN:
            op = OP_PLUS_ASSIGN;
            bin_op = OP_PLUS;
            is_compound = true;
            break;
        case TOKEN_SUB_ASSIGN:
            op = OP_MINUS_ASSIGN;
            bin_op = OP_MINUS;
            is_compound = true;
            break;
        case TOKEN_MUL_ASSIGN:
            op = OP_MUL_ASSIGN;
            bin_op = OP_MUL;
            is_compound = true;
            break;
        case TOKEN_DIV_ASSIGN:
            op = OP_DIV_ASSIGN;
            bin_op = OP_DIV;
            is_compound = true;
            break;
        default:
            syntax_error(L, "assignment operator");
        }
        getNextToken(L);
        Expression *right = parse_assignment(L, currentFunc);
        if (is_compound)
        {
            Expression *left_copy = my_malloc(sizeof(Expression));
            *left_copy = *left;
            left_copy->left = left_copy->right = NULL;
            Expression *bin_exp = make_binary(left_copy, bin_op, right, L->lineno);
            left = make_assignment(left, bin_exp, L->lineno, OP_ASSIGN);
        }
        else
        {
            left = make_assignment(left, right, L->lineno, op);
        }
    }
    return left;
}

// Parse ternary operator: cond ? expr : expr
Expression *parse_ternary(lexer *L, Function *currentFunc)
{
    Expression *cond = parse_logicalOr(L, currentFunc);
    if (L->current.ID == TOKEN_QUESTION)
    {
        getNextToken(L);
        Expression *trueExpr = parse_assignment(L, currentFunc);
        if (L->current.ID != TOKEN_COLON)
            syntax_error(L, "':' in ternary operator");
        getNextToken(L);
        Expression *falseExpr = parse_assignment(L, currentFunc);
        Expression *node = my_malloc(sizeof(Expression));
        node->kind = EXPR_TERNARY;
        node->lineno = cond->lineno;
        node->left = cond;
        node->right = NULL;
        node->numArgs = 0;
        node->args = my_malloc(2 * sizeof(Expression *));
        node->args[0] = trueExpr;
        node->args[1] = falseExpr;
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
Expression *parse_logicalOr(lexer *L, Function *currentFunc)
{
    Expression *node = parse_logicalAnd(L, currentFunc);
    while (L->current.ID == TOKEN_OR)
    {

        getNextToken(L);
        Expression *right = parse_logicalAnd(L, currentFunc);
        node = make_binary(node, OP_OR, right, L->lineno);
        node->exprType.base = BASE_INT;
        node->exprType.isArray = false;
        node->exprType.isConst = false;
    }
    return node;
}

// Parse logical AND: left && right
Expression *parse_logicalAnd(lexer *L, Function *currentFunc)
{
    Expression *node = parse_equality(L, currentFunc);
    while (L->current.ID == TOKEN_AMPERSAND && strcmp(L->current.attrb, "&&") == 0)
    {

        getNextToken(L);
        Expression *right = parse_equality(L, currentFunc);
        node = make_binary(node, OP_AND, right, L->lineno);
        node->exprType.base = BASE_INT;
        node->exprType.isArray = false;
        node->exprType.isConst = false;
    }
    return node;
}

// Parse equality operators: == and !=
Expression *parse_equality(lexer *L, Function *currentFunc)
{
    Expression *node = parse_relational(L, currentFunc);
    while (L->current.ID == TOKEN_EQ || L->current.ID == TOKEN_NE)
    {

        Operator oper = (L->current.ID == TOKEN_EQ) ? OP_EQ : OP_NE;
        getNextToken(L);
        Expression *right = parse_relational(L, currentFunc);
        node = make_binary(node, oper, right, L->lineno);
        /* Result of equality is int */
        node->exprType.base = BASE_INT;
        node->exprType.isArray = false;
        node->exprType.isConst = false;
    }
    return node;
}

// Parse relational operators: <, <=, >, >=
Expression *parse_relational(lexer *L, Function *currentFunc)
{
    Expression *node = parse_additive(L, currentFunc);
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
        Expression *right = parse_additive(L, currentFunc);
        node = make_binary(node, oper, right, L->lineno);
        node->exprType.base = BASE_INT;
        node->exprType.isArray = false;
        node->exprType.isConst = false;
    }
    return node;
}

// Parse addition: left + right or left - right
Expression *parse_additive(lexer *L, Function *currentFunc)
{
    Expression *node = parse_multiplicative(L, currentFunc);
    while (L->current.ID == TOKEN_PLUS || L->current.ID == TOKEN_MINUS)
    {

        Operator oper = (L->current.ID == TOKEN_PLUS) ? OP_PLUS : OP_MINUS;
        getNextToken(L);
        Expression *right = parse_multiplicative(L, currentFunc);
        node = make_binary(node, oper, right, L->lineno);
    }
    return node;
}

// Parse multiplication: left * right or left / right or left % right
Expression *parse_multiplicative(lexer *L, Function *currentFunc)
{
    Expression *node = parse_unary(L, currentFunc);
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
        Expression *right = parse_unary(L, currentFunc);
        node = make_binary(node, oper, right, L->lineno);
    }
    return node;
}

Expression *parse_unary(lexer *L, Function *currentFunc)
{
    if (L->current.ID == TOKEN_MINUS)
    {
        getNextToken(L);
        Expression *operand = parse_unary(L, currentFunc);
        return make_unary(OP_NEG, operand, L->lineno);
    }
    if (L->current.ID == TOKEN_EXCLAMATION)
    {
        getNextToken(L);
        Expression *operand = parse_unary(L, currentFunc);
        return make_unary(OP_NOT, operand, L->lineno);
    }
    if (L->current.ID == TOKEN_INC) // Handle ++
    {
        getNextToken(L);
        Expression *operand = parse_unary(L, currentFunc);
        return make_unary(OP_INC, operand, L->lineno);
    }
    if (L->current.ID == TOKEN_TILDE)
    {
        getNextToken(L);
        Expression *operand = parse_unary(L, currentFunc);
        return make_unary(OP_TILDE, operand, L->lineno);
    }
    if (L->current.ID == TOKEN_DEC) // Handle --
    {
        getNextToken(L);
        Expression *operand = parse_unary(L, currentFunc);
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
            Expression *expr = parse_unary(L, currentFunc);
            return make_cast(castType, expr, L->lineno);
        }
        else
        {
            Expression *expr = parse_assignment(L, currentFunc);
            if (L->current.ID != TOKEN_RPAREN)
                syntax_error(L, "')'");
            getNextToken(L);
            return expr;
        }
    }
    return parse_primary(L, currentFunc);
}

Expression *parse_primary(lexer *L, Function *currentFunc)
{

    Expression *node = NULL;
    if (L->current.ID == TOKEN_LPAREN)
    {
        getNextToken(L);
        node = parse_assignment(L, currentFunc);
        if (L->current.ID != TOKEN_RPAREN)
            syntax_error(L, "')'");
        getNextToken(L);
    }
    else if (L->current.ID == TOKEN_INT || L->current.ID == TOKEN_REAL ||
             L->current.ID == TOKEN_CHAR || L->current.ID == TOKEN_STRING)
    {
        node = make_literal(L->current.attrb,
                            (L->current.ID == TOKEN_INT) ? (Type){BASE_INT, false, false, "", L->lineno} : (L->current.ID == TOKEN_REAL) ? (Type){BASE_FLOAT, false, false, "", L->lineno}
                                                                                                       : (L->current.ID == TOKEN_CHAR)   ? (Type){BASE_CHAR, false, false, "", L->lineno}
                                                                                                                                         : (Type){BASE_CHAR, true, true, "", L->lineno},
                            L->lineno);
        getNextToken(L);
    }
    else if (L->current.ID == TOKEN_IDENTIFIER)
    {
        node = make_identifier(L->current.attrb, L->lineno, currentFunc);
        getNextToken(L);

        while (L->current.ID == TOKEN_LBRACKET)
        {
            getNextToken(L);
            Expression *indexExpr = parse_assignment(L, currentFunc);
            if (L->current.ID != TOKEN_RBRACKET)
                syntax_error(L, "']'");
            getNextToken(L);
            node = make_index(node, indexExpr, L->lineno);
        }
        if (L->current.ID == TOKEN_LPAREN)
        {
            getNextToken(L);
            int numArgs = 0;
            Expression **args = NULL;
            if (L->current.ID != TOKEN_RPAREN)
            {
                args = my_malloc(10 * sizeof(Expression *));
                while (1)
                {
                    args[numArgs++] = parse_assignment(L, currentFunc);
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
            getNextToken(L);
            node = make_call(node, args, numArgs, L->lineno);
        }
        while (L->current.ID == TOKEN_DOT)
        {
            getNextToken(L);
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

Statement *parser_declaration(lexer *L, LookaheadBuffer *buf, bool isGlobal, bool isConst, Function *currentFunc)
{
    int buf_pos = 0;
    Statement *stmt = my_malloc(sizeof(Statement));
    stmt->kind = STMT_DECL;
    stmt->lineno = L->lineno;

    Declaration *decl = &stmt->u.decl;
    memset(decl, 0, sizeof(Declaration));

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

        else
        {
            syntax_error(L, "type specifier");
        }
    }
    else if (t.ID == TOKEN_STRUCT)
    {
        type.base = BASE_STRUCT;
        t = (buf_pos < buf->count) ? buf->tokens[buf_pos++] : L->current;
        if (t.ID != TOKEN_IDENTIFIER)
            syntax_error(L, "struct name");
        if (!lookup_struct(buf->tokens[0].attrb))
            syntax_error(L, "unknown struct");
        strncpy(type.structName, t.attrb, sizeof(type.structName) - 1);
        strncpy(decl->name, t.attrb, sizeof(decl->name) - 1);
    }

    else
    {
        syntax_error(L, "type specifier");
    }

    type.isConst = isConst;
    type.isArray = false;
    t = (buf_pos < buf->count) ? buf->tokens[buf_pos++] : L->current;
    if (type.base != BASE_STRUCT)
    {
        if (t.ID != TOKEN_IDENTIFIER)
            syntax_error(L, "variable name");
        strncpy(decl->name, t.attrb, sizeof(decl->name) - 1);
    }
    if (L->current.ID == TOKEN_LBRACKET)
    {
        getNextToken(L);
        while (L->current.ID != TOKEN_RBRACKET && L->current.ID != END)
            getNextToken(L);

        if (L->current.ID != TOKEN_RBRACKET)
            syntax_error(L, "']'");
        getNextToken(L);
        type.isArray = true;
    }
    type.lineDeclared = L->lineno;
    decl->declType = type;
    add_variable(L, decl->name, type, isGlobal, currentFunc);
    if (L->current.ID == TOKEN_EQUAL)
    {
        getNextToken(L);
        decl->init = parse_assignment(L, currentFunc);
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
Statement *parser_statement(lexer *L, LookaheadBuffer *buf, bool isGlobal, bool isConst, Function *currentFunc)
{
    Statement *stmt = NULL;

    if (buf && buf->count > 0)
    {
        token t = buf->tokens[0];
        if (t.ID == TOKEN_TYPE || t.ID == TOKEN_STRUCT)
        {
            if (L->current.ID != TOKEN_LBRACKET && L->current.ID != TOKEN_SEMICOLON)
                getNextToken(L);
            stmt = parser_declaration(L, buf, isGlobal, isConst, currentFunc);
            return stmt;
        }
    }
    if (L->current.ID == TOKEN_IF)
    {
        stmt = parse_if(L, currentFunc);
        return stmt;
    }
    else if (L->current.ID == TOKEN_WHILE)
    {
        stmt = parse_while(L, currentFunc);
        return stmt;
    }
    else if (L->current.ID == TOKEN_DO)
    {
        stmt = parse_do(L, currentFunc);
        return stmt;
    }
    else if (L->current.ID == TOKEN_FOR)
    {
        stmt = parse_for(L, currentFunc);
        return stmt;
    }
    else if (L->current.ID == TOKEN_BREAK)
    {
        stmt = parse_break(L);
        return stmt;
    }
    else if (L->current.ID == TOKEN_CONTINUE)
    {
        stmt = parse_continue(L);
        return stmt;
    }
    else if (L->current.ID == TOKEN_RETURN)
    {
        stmt = my_malloc(sizeof(Statement));
        stmt->kind = STMT_RETURN;
        stmt->lineno = L->lineno;
        getNextToken(L);
        Expression *retExpr = NULL;
        if (L->current.ID != TOKEN_SEMICOLON)
        {
            retExpr = parse_assignment(L, currentFunc);
        }
        stmt->u.expr = retExpr;
        if (L->current.ID != TOKEN_SEMICOLON)
            syntax_error(L, "';'");
        getNextToken(L);
        return stmt;
    }

    else if (L->current.ID == TOKEN_LBRACE)
    {
        return parse_compound(L, currentFunc);
    }

    stmt = my_malloc(sizeof(Statement));
    stmt->kind = STMT_EXPR;
    stmt->lineno = L->lineno;
    stmt->u.expr = NULL;
    if (L->current.ID == TOKEN_RBRACE)
        return stmt;
    stmt->u.expr = parse_assignment(L, currentFunc);
    return stmt;
}

Statement *parse_if(lexer *L, Function *currentFunc)
{

    if (L->current.ID != TOKEN_IF)
        syntax_error(L, "if");
    int lineno = L->lineno;
    getNextToken(L);
    if (L->current.ID != TOKEN_LPAREN)
        syntax_error(L, "'('");
    getNextToken(L);
    Expression *condition = parse_assignment(L, currentFunc);
    if (L->current.ID != TOKEN_RPAREN)
        syntax_error(L, "')'");
    getNextToken(L);
    Statement *ifbody = parser_statement(L, NULL, false, false, currentFunc);
    Statement *elsestmt = NULL;
    if (L->current.ID == TOKEN_ELSE)
    {
        getNextToken(L);
        elsestmt = parser_statement(L, NULL, false, false, currentFunc);
    }
    Statement *stmt = my_malloc(sizeof(Statement));
    stmt->kind = STMT_IF;
    stmt->lineno = lineno;
    stmt->u.ifstmt = my_malloc(sizeof(IfStmt));
    stmt->u.ifstmt->condition = condition;
    stmt->u.ifstmt->thenstmt = ifbody;
    stmt->u.ifstmt->elsestmt = elsestmt;
    stmt->numStmts = 0;
    stmt->stmts = NULL;
    return stmt;
}

Statement *parse_while(lexer *L, Function *currentFunc)
{
    if (L->current.ID != TOKEN_WHILE)
        syntax_error(L, "while");
    int lineno = L->lineno;
    getNextToken(L);
    if (L->current.ID != TOKEN_LPAREN)
        syntax_error(L, "'('");
    getNextToken(L);
    Expression *condition = parse_assignment(L, currentFunc);
    if (L->current.ID != TOKEN_RPAREN)
        syntax_error(L, "')'");
    getNextToken(L);
    Statement *body = parser_statement(L, NULL, false, false, currentFunc);
    Statement *stmt = my_malloc(sizeof(Statement));
    stmt->kind = STMT_WHILE;
    stmt->lineno = lineno;
    stmt->u.whilestmt = my_malloc(sizeof(WhileStmt));
    stmt->u.whilestmt->condition = condition;
    stmt->u.whilestmt->body = body;
    stmt->numStmts = 0;
    stmt->stmts = NULL;
    return stmt;
}

Statement *parse_do(lexer *L, Function *currentFunc)
{
    if (L->current.ID != TOKEN_DO)
        syntax_error(L, "do");
    int lineno = L->lineno;
    getNextToken(L);
    Statement *body = parser_statement(L, NULL, false, false, currentFunc);
    if (L->current.ID != TOKEN_WHILE)
        syntax_error(L, "while");
    getNextToken(L);
    if (L->current.ID != TOKEN_LPAREN)
        syntax_error(L, "'('");
    getNextToken(L);
    Expression *condition = parse_assignment(L, currentFunc);
    if (L->current.ID != TOKEN_RPAREN)
        syntax_error(L, "')'");
    getNextToken(L);
    if (L->current.ID != TOKEN_SEMICOLON)
        syntax_error(L, "';'");
    getNextToken(L);
    Statement *stmt = my_malloc(sizeof(Statement));
    stmt->kind = STMT_DO;
    stmt->lineno = lineno;
    stmt->u.dostmt = my_malloc(sizeof(DoStmt));
    stmt->u.dostmt->body = body;
    stmt->u.dostmt->condition = condition;
    stmt->numStmts = 0;
    stmt->stmts = NULL;
    return stmt;
}

Statement *parse_for(lexer *L, Function *currentFunc)
{
    if (L->current.ID != TOKEN_FOR)
        syntax_error(L, "for");
    int lineno = L->lineno;
    getNextToken(L);
    if (L->current.ID != TOKEN_LPAREN)
        syntax_error(L, "'('");
    getNextToken(L);
    Statement *init = NULL;
    if (L->current.ID != TOKEN_SEMICOLON)
    {
        LookaheadBuffer buf;
        init_lookahead(&buf);
        push_token(&buf, L->current);
        if (L->current.ID == TOKEN_TYPE || L->current.ID == TOKEN_STRUCT)
        {
            getNextToken(L);
            push_token(&buf, L->current);
            init = parser_declaration(L, &buf, false, false, currentFunc);
        }
        else
        {
            init = parser_statement(L, NULL, false, false, currentFunc);
        }
    }

    getNextToken(L);

    Expression *condition = NULL;
    if (L->current.ID != TOKEN_SEMICOLON)
    {
        condition = parse_assignment(L, currentFunc);
    }
    if (L->current.ID != TOKEN_SEMICOLON)
        syntax_error(L, "';'");
    getNextToken(L);
    Expression *update = NULL;

    if (L->current.ID != TOKEN_RPAREN)
    {
        update = parse_assignment(L, currentFunc);
    }

    if (L->current.ID != TOKEN_RPAREN)
        syntax_error(L, "')'");
    getNextToken(L);
    Statement *body = parse_compound(L, currentFunc);
    Statement *stmt = my_malloc(sizeof(Statement));
    stmt->kind = STMT_FOR;
    stmt->lineno = lineno;
    stmt->u.forstmt = my_malloc(sizeof(ForStmt));
    stmt->u.forstmt->init = init;
    stmt->u.forstmt->condition = condition;
    stmt->u.forstmt->update = update;
    stmt->u.forstmt->body = body;
    stmt->numStmts = 0;
    stmt->stmts = NULL;
    return stmt;
}

Statement *parse_break(lexer *L)
{
    if (L->current.ID != TOKEN_BREAK)
        syntax_error(L, "break");
    int lineno = L->lineno;
    getNextToken(L);
    if (L->current.ID != TOKEN_SEMICOLON)
        syntax_error(L, "';'");
    getNextToken(L);
    Statement *stmt = my_malloc(sizeof(Statement));
    stmt->kind = STMT_BREAK;
    stmt->lineno = lineno;
    stmt->numStmts = 0;
    stmt->stmts = NULL;
    return stmt;
}

Statement *parse_continue(lexer *L)
{
    if (L->current.ID != TOKEN_CONTINUE)
        syntax_error(L, "continue");
    int lineno = L->lineno;
    getNextToken(L);
    if (L->current.ID != TOKEN_SEMICOLON)
        syntax_error(L, "';'");
    getNextToken(L);
    Statement *stmt = my_malloc(sizeof(Statement));
    stmt->kind = STMT_CONTINUE;
    stmt->lineno = lineno;
    stmt->numStmts = 0;
    stmt->stmts = NULL;
    return stmt;
}

Statement *parse_compound(lexer *L, Function *currentFunc)
{
    if (L->current.ID != TOKEN_LBRACE)
        syntax_error(L, "'{'");
    getNextToken(L);
    Statement **stmts = my_malloc(100 * sizeof(Statement *));
    int lineno = L->lineno;
    int count = 0;
    LookaheadBuffer buf;
    init_lookahead(&buf);
    bool isConst;
    bool isStruct;
    bool parsingStruct;
    token save_struct;
    while (L->current.ID != TOKEN_RBRACE)
    {

        clear_lookahead(&buf);
        isConst = false;
        isStruct = false;
        parsingStruct = false;
        if (L->current.ID == TOKEN_CONST)
        {
            isConst = true;
            getNextToken(L);
        }
        if (L->current.ID == TOKEN_TYPE || L->current.ID == TOKEN_STRUCT)
        {
            if (L->current.ID == TOKEN_STRUCT)
            {
                isStruct = true;
                save_struct = L->current;
                getNextToken(L);
            }

            push_token(&buf, L->current);
            getNextToken(L);
            if (L->current.ID == TOKEN_CONST)
            {
                if (isConst)
                    syntax_error(L, "type");
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
            stmts[count++] = parser_statement(L, NULL, false, isConst, currentFunc);
        }
        else if (isStruct && buf.tokens[0].ID == TOKEN_IDENTIFIER && L->current.ID == TOKEN_LBRACE)
        {
            push_token(&buf, buf.tokens[0]);
            push_token(&buf, L->current);
            buf.tokens[0] = save_struct;
            stmts[count++] = parse_struct(L, &buf);
            parsingStruct = true;
        }
        else
        {
            bool flag = true;
            do
            {
                if (isStruct)
                {
                    buf.tokens[0].ID = TOKEN_STRUCT;
                    append(&struct_table, &size, &capacity, buf.tokens[1].attrb, buf.tokens[0].attrb);
                }

                if (L->current.ID != TOKEN_LBRACKET && L->current.ID != TOKEN_SEMICOLON)
                {

                    if (((buf.tokens[0].ID == TOKEN_TYPE || buf.tokens[0].ID == TOKEN_STRUCT) && buf.tokens[1].ID == TOKEN_IDENTIFIER))
                        stmts[count++] = parser_statement(L, &buf, false, isConst, currentFunc);
                    if (L->current.ID == TOKEN_IDENTIFIER)
                    {

                        buf.tokens[1] = L->current;
                        getNextToken(L);
                        if (L->current.ID == TOKEN_EQUAL)
                        {
                            stmts[count++] = parser_statement(L, &buf, false, isConst, currentFunc);
                            getNextToken(L);
                            flag = false;
                        }
                        else
                        {
                            flag = false;
                            if (L->current.ID == TOKEN_LBRACKET || L->current.ID == TOKEN_LPAREN || L->current.ID == TOKEN_SEMICOLON)
                                stmts[count++] = parser_statement(L, &buf, false, isConst, currentFunc);
                        }
                    }
                }
                if (flag)
                {
                    stmts[count++] = parser_statement(L, &buf, false, isConst, currentFunc);
                }

            } while (L->current.ID == TOKEN_COMMA);
        }
        if ((!parsingStruct && (stmts[count - 1]->kind == STMT_DECL || stmts[count - 1]->kind == STMT_EXPR)) && L->current.ID != TOKEN_RBRACE)
        {
            if (L->current.ID != TOKEN_SEMICOLON)
                syntax_error(L, "';'");
            getNextToken(L);
        }
    }
    if (L->current.ID != TOKEN_RBRACE)
        syntax_error(L, "'}'");
    getNextToken(L);
    Statement *compound = my_malloc(sizeof(Statement));
    compound->kind = STMT_COMPOUND;
    compound->lineno = lineno;
    compound->numStmts = count;
    compound->stmts = stmts;
    return compound;
}

Statement *parse_function_declaration(lexer *L, LookaheadBuffer *buf)
{
    int buf_pos = 0;

    bool isConst = false;
    token t = (buf_pos < buf->count) ? buf->tokens[buf_pos++] : L->current;
    if (t.ID == TOKEN_CONST)
    {
        isConst = true;
        if (buf_pos <= buf->count)
            getNextToken(L);
        t = (buf_pos < buf->count) ? buf->tokens[buf_pos++] : L->current;
    }

    Type retType = {0};
    int retTypeLine = L->lineno;
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
    retType.lineDeclared = retTypeLine;

    // Parse function name
    t = (buf_pos < buf->count) ? buf->tokens[buf_pos++] : L->current;
    if (t.ID != TOKEN_IDENTIFIER)
        syntax_error(L, "function name");
    char funcName[64];
    strncpy(funcName, t.attrb, sizeof(funcName) - 1);
    int funcNameLine = L->lineno;
    if (buf_pos <= buf->count)
        getNextToken(L);

    t = (buf_pos < buf->count) ? buf->tokens[buf_pos++] : L->current;
    if (t.ID != TOKEN_LPAREN)
        syntax_error(L, "'(' after function name");

    // Parse parameter list
    VarSymbol **params = my_malloc(20 * sizeof(VarSymbol *));
    int numParams = 0;
    LookaheadBuffer buff;
    init_lookahead(&buff);
    Function *func = my_malloc(sizeof(Function));
    func->locals = NULL;
    while (L->current.ID != TOKEN_RPAREN)
    {

        clear_lookahead(&buff);
        bool pConst = false;
        if (L->current.ID == TOKEN_CONST)
        {
            pConst = true;
            getNextToken(L);
        }
        push_token(&buff, L->current);
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
        pType.lineDeclared = L->lineno;
        getNextToken(L);
        push_token(&buff, L->current);

        if (L->current.ID != TOKEN_IDENTIFIER)
            syntax_error(L, "parameter name");
        char paramName[64];
        strncpy(paramName, L->current.attrb, sizeof(paramName) - 1);
        getNextToken(L);
        if (L->current.ID == TOKEN_LBRACKET)
        {
            getNextToken(L);
            while (L->current.ID != TOKEN_RBRACKET && L->current.ID != END)
                getNextToken(L);
            if (L->current.ID != TOKEN_RBRACKET)
                syntax_error(L, "']'");
            getNextToken(L);
            pType.isArray = true;
        }
        VarSymbol *vs = my_malloc(sizeof(VarSymbol));
        strncpy(vs->name, paramName, sizeof(vs->name) - 1);
        vs->type = pType;
        vs->isGlobal = false;
        vs->localIndex = -1;
        vs->next = func->locals;
        func->locals = vs;
        params[numParams] = vs;
        numParams++;

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

    Function *existing = lookup_function(funcName);
    if (existing)
    {
        if (!equal_types(&existing->returnType, &retType))
        {
            char prevRetType[56], newRetType[56];
            format_type(&existing->returnType, prevRetType, sizeof(prevRetType));
            format_type(&retType, newRetType, sizeof(newRetType));
            char msg[256];
            snprintf(msg, sizeof(msg), "Prototype %s %s(...) differs from previous declaration at file %s line %d",
                     newRetType, funcName, L->filename, retTypeLine);
            type_error(L->filename, funcNameLine, msg);
        }
        if (existing->numParams != numParams)
        {
            char msg[256];
            snprintf(msg, sizeof(msg), "Prototype void %s(...) differs from previous declaration at file %s line %d",
                     funcName, L->filename, retTypeLine);
            type_error(L->filename, funcNameLine, msg);
        }
        // Compare parameter types
        for (int i = 0; i < numParams; i++)
        {
            if (!equal_types(&existing->params[i]->type, &params[i]->type))
            {
                char prevParamType[52], newParamType[52];
                format_type(&existing->params[i]->type, prevParamType, sizeof(prevParamType));
                format_type(&params[i]->type, newParamType, sizeof(newParamType));
                char msg[256];
                snprintf(msg, sizeof(msg), "Prototype void %s(%s) differs from previous declaration at file %s line %d",
                         funcName, newParamType, L->filename, retTypeLine);
                type_error(L->filename, funcNameLine, msg);
            }
        }
        if (existing->defined)
        {
            char msg[256];
            snprintf(msg, sizeof(msg), "Redefinition of function %s, previously defined at file %s line %d",
                     funcName, L->filename, retTypeLine);
            type_error(L->filename, funcNameLine, msg);
        }
    }

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
        Statement *body = parse_compound(L, func);
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
    if (t.ID != TOKEN_STRUCT)
        syntax_error(L, "struct");

    t = (buf_pos < buf->count) ? buf->tokens[buf_pos++] : L->current;
    if (t.ID != TOKEN_IDENTIFIER)
        syntax_error(L, "struct name");
    char structName[64];
    strncpy(structName, t.attrb, sizeof(structName) - 1);

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

    LookaheadBuffer member_buf;
    init_lookahead(&member_buf);
    bool isConst;
    while (L->current.ID != TOKEN_RBRACE)
    {
        clear_lookahead(&member_buf);
        isConst = false;
        if (L->current.ID == TOKEN_CONST)
        {
            isConst = true;
            getNextToken(L);
        }
        push_token(&member_buf, L->current);
        if (L->current.ID != TOKEN_TYPE)
            syntax_error(L, "type specifier in struct member");

        getNextToken(L);
        if (L->current.ID == TOKEN_CONST)
        {
            if (isConst)
                syntax_error(L, "type");
            isConst = true;
            getNextToken(L);
        }
        if (L->current.ID == TOKEN_STRUCT)
        {
            syntax_error(L, "non struct inside struct");
        }
        push_token(&member_buf, L->current);
        if (L->current.ID != TOKEN_IDENTIFIER)
        {
            syntax_error(L, "variable");
        }
        getNextToken(L);
        do
        {

            if (L->current.ID != TOKEN_LBRACKET)
            {
                if (L->current.ID == TOKEN_IDENTIFIER)
                {
                    member_buf.tokens[1] = L->current;
                    getNextToken(L);
                }
                if (member_buf.tokens[0].ID == TOKEN_TYPE && member_buf.tokens[1].ID == TOKEN_IDENTIFIER)
                {
                    Statement *memberStmt = parser_declaration(L, &member_buf, true, isConst, NULL);
                    sdef->members[sdef->numMembers] = my_malloc(sizeof(Declaration));
                    *(sdef->members[sdef->numMembers]) = memberStmt->u.decl;
                    sdef->numMembers++;

                    free(memberStmt);
                }
            }
            if (L->current.ID == TOKEN_COMMA)
                getNextToken(L);
            else
                break;

        } while (1);

        if (L->current.ID != TOKEN_SEMICOLON)
            syntax_error(L, "';' after struct member declaration");
        getNextToken(L); // consume ';'
    }

    if (L->current.ID != TOKEN_RBRACE)
        syntax_error(L, "'}' after struct definition");
    getNextToken(L); // consume '}'

    if (L->current.ID != TOKEN_SEMICOLON)
        syntax_error(L, "';' after struct definition");
    getNextToken(L); // consume ';'

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
    init_symbol_tables();
    Statement **stmts = my_malloc(200 * sizeof(Statement *));
    int count = 0;
    LookaheadBuffer buf;
    init_lookahead(&buf);
    bool isConst;
    while (L->current.ID != END)
    {

        clear_lookahead(&buf);
        isConst = false;
        if (L->current.ID == TOKEN_CONST)
        {
            isConst = true;
            getNextToken(L);
        }
        push_token(&buf, L->current);
        if (buf.count > 0 && (buf.tokens[0].ID == TOKEN_TYPE ||
                              (buf.tokens[0].ID == TOKEN_STRUCT)))
        {
            getNextToken(L);
            if (L->current.ID == TOKEN_CONST)
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
                if (buf.tokens[0].ID == TOKEN_STRUCT &&
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

                            if (((buf.tokens[0].ID == TOKEN_TYPE || buf.tokens[0].ID == TOKEN_STRUCT) && buf.tokens[1].ID == TOKEN_IDENTIFIER))
                                stmts[count++] = parser_statement(L, &buf, true, isConst, NULL);
                            if (L->current.ID == TOKEN_IDENTIFIER)
                            {

                                buf.tokens[1] = L->current;
                            }
                        }
                        stmts[count++] = parser_statement(L, &buf, true, isConst, NULL);

                    } while (L->current.ID == TOKEN_COMMA);

                    if (L->current.ID != TOKEN_SEMICOLON)
                    {
                        syntax_error(L, "';'");
                    }

                    getNextToken(L);
                }
            }
            else if (buf.tokens[0].ID == TOKEN_STRUCT)
            {
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
            stmts[count++] = parser_statement(L, NULL, true, isConst, NULL);
        }
    }

    *stmtCount = count;
    return stmts;
}

void type_check_expression(Expression *expr, const char *filename, FILE *output, Function *currentFunc)
{
    if (!expr)
        return;
    switch (expr->kind)
    {
    case EXPR_LITERAL:
        break;
    case EXPR_IDENTIFIER:
    {
        VarSymbol *vs = lookup_variable(expr->value, currentFunc);
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
        type_check_expression(expr->right, filename, output, currentFunc);
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
            expr->exprType = (Type){BASE_INT, false, false, "", expr->lineno};
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
        type_check_expression(expr->left, filename, output, currentFunc);
        type_check_expression(expr->right, filename, output, currentFunc);
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
                         leftStr, expr->op == OP_PLUS ? "+" : expr->op == OP_MINUS ? "-"
                                                          : expr->op == OP_MUL     ? "*"
                                                          : expr->op == OP_DIV     ? "/"
                                                                                   : "%",
                         rightStr);
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
            expr->exprType = (Type){BASE_INT, false, false, "", expr->lineno};
        }
        else if (expr->op == OP_AND || expr->op == OP_OR)
        {
            expr->exprType = (Type){BASE_INT, false, false, "", expr->lineno};
        }
    }
    break;
    case EXPR_ASSIGN:
    {
        type_check_expression(expr->left, filename, output, currentFunc);
        type_check_expression(expr->right, filename, output, currentFunc);
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
        type_check_expression(expr->left, filename, output, currentFunc);
        for (int i = 0; i < expr->numArgs; i++)
            type_check_expression(expr->args[i], filename, output, currentFunc);
        expr->exprType = expr->left->exprType;
    }
    break;
    case EXPR_INDEX:
    {
        type_check_expression(expr->left, filename, output, currentFunc);
        type_check_expression(expr->right, filename, output, currentFunc);
        expr->exprType = expr->left->exprType;
        expr->exprType.isArray = false;
    }
    break;
    case EXPR_MEMBER:
    {
        type_check_expression(expr->left, filename, output, currentFunc);
    }
    break;
    case EXPR_TERNARY:
    {
        type_check_expression(expr->left, filename, output, currentFunc);
        type_check_expression(expr->args[0], filename, output, currentFunc);
        type_check_expression(expr->args[1], filename, output, currentFunc);
        expr->exprType = expr->args[0]->exprType;
    }
    break;
    case EXPR_CAST:
    {
        type_check_expression(expr->left, filename, output, currentFunc);
    }
    break;
    }
}

void type_check_statement(Statement *stmt, const char *filename, FILE *output, bool inLoop)
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
            type_check_expression(stmt->u.expr, filename, output, func);
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
            type_check_expression(stmt->u.decl.init, filename, output, func);
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

        Type actualType = stmt->u.expr ? stmt->u.expr->exprType : (Type){BASE_VOID, false, false, "", stmt->lineno};
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
            type_check_expression(stmt->u.expr, filename, output, func);
        break;
    case STMT_COMPOUND:
        for (int i = 0; i < stmt->numStmts; i++)
            type_check_statement(stmt->stmts[i], filename, output, inLoop);
        break;

    case STMT_IF:
        type_check_expression(stmt->u.ifstmt->condition, filename, output, func);
        if (!is_numeric_type(&stmt->u.ifstmt->condition->exprType))
        {
            char typeStr[128];
            format_type(&stmt->u.ifstmt->condition->exprType, typeStr, sizeof(typeStr));
            char msg[256];
            snprintf(msg, sizeof(msg), "If condition must be numeric, was %s", typeStr);
            type_error(filename, stmt->lineno, msg);
        }
        type_check_statement(stmt->u.ifstmt->thenstmt, filename, output, inLoop);
        if (stmt->u.ifstmt->elsestmt)
            type_check_statement(stmt->u.ifstmt->elsestmt, filename, output, inLoop);
        break;
    case STMT_WHILE:
        type_check_expression(stmt->u.whilestmt->condition, filename, output, func);
        if (!is_numeric_type(&stmt->u.whilestmt->condition->exprType))
        {
            char typeStr[128];
            format_type(&stmt->u.whilestmt->condition->exprType, typeStr, sizeof(typeStr));
            char msg[256];
            snprintf(msg, sizeof(msg), "While condition must be numeric, was %s", typeStr);
            type_error(filename, stmt->lineno, msg);
        }
        type_check_statement(stmt->u.whilestmt->body, filename, output, true);
        break;
    case STMT_DO:
        type_check_expression(stmt->u.dostmt->condition, filename, output, func);
        if (!is_numeric_type(&stmt->u.dostmt->condition->exprType))
        {
            char typeStr[128];
            format_type(&stmt->u.dostmt->condition->exprType, typeStr, sizeof(typeStr));
            char msg[256];
            snprintf(msg, sizeof(msg), "Do-while condition must be numeric, was %s", typeStr);
            type_error(filename, stmt->lineno, msg);
        }
        type_check_statement(stmt->u.dostmt->body, filename, output, true);
        break;
    case STMT_FOR:
        if (stmt->u.forstmt->init)
            type_check_statement(stmt->u.forstmt->init, filename, output, inLoop);
        if (stmt->u.forstmt->condition)
        {
            type_check_expression(stmt->u.forstmt->condition, filename, output, func);
            if (!is_numeric_type(&stmt->u.forstmt->condition->exprType))
            {
                char typeStr[128];
                format_type(&stmt->u.forstmt->condition->exprType, typeStr, sizeof(typeStr));
                char msg[256];
                snprintf(msg, sizeof(msg), "For condition must be numeric, was %s", typeStr);
                type_error(filename, stmt->lineno, msg);
            }
        }
        if (stmt->u.forstmt->update)
            type_check_expression(stmt->u.forstmt->update, filename, output, func);
        type_check_statement(stmt->u.forstmt->body, filename, output, true);
        break;
    case STMT_BREAK:
        if (!inLoop)
        {
            type_error(filename, stmt->lineno, "break not inside a loop");
        }
        break;
    case STMT_CONTINUE:
        if (!inLoop)
        {
            type_error(filename, stmt->lineno, "continue not inside a loop");
        }
        break;
    }
}

void init_symbol_tables()
{
    // Predeclare lib440 functions
    Type voidType = {BASE_VOID, false, false, "", 0};
    Type intType = {BASE_INT, false, false, "", 0};
    Type floatType = {BASE_FLOAT, false, false, "", 0};
    Type charArrayType = {BASE_CHAR, false, true, "", 0};

    // putint(int) -> void
    VarSymbol *putint_param = my_malloc(sizeof(VarSymbol));
    strncpy(putint_param->name, "x", sizeof(putint_param->name) - 1);
    putint_param->type = intType;
    putint_param->isGlobal = false;
    putint_param->localIndex = -1;
    putint_param->next = NULL;
    add_function("putint", voidType, 1, &putint_param, false);

    // putchar(int) -> int
    VarSymbol *putchar_param = my_malloc(sizeof(VarSymbol));
    strncpy(putchar_param->name, "x", sizeof(putchar_param->name) - 1);
    putchar_param->type = intType;
    putchar_param->isGlobal = false;
    putchar_param->localIndex = -1;
    putchar_param->next = NULL;
    add_function("putchar", intType, 1, &putchar_param, false);

    // putfloat(float) -> void
    VarSymbol *putfloat_param = my_malloc(sizeof(VarSymbol));
    strncpy(putfloat_param->name, "x", sizeof(putfloat_param->name) - 1);
    putfloat_param->type = floatType;
    putfloat_param->isGlobal = false;
    putfloat_param->localIndex = -1;
    putfloat_param->next = NULL;
    add_function("putfloat", voidType, 1, &putfloat_param, false);

    // getint() -> int
    add_function("getint", intType, 0, NULL, false);

    // getchar() -> int
    add_function("getchar", intType, 0, NULL, false);

    // getfloat() -> float
    add_function("getfloat", floatType, 0, NULL, false);

    // putstring(char[]) -> void
    VarSymbol *putstring_param = my_malloc(sizeof(VarSymbol));
    strncpy(putstring_param->name, "s", sizeof(putstring_param->name) - 1);
    putstring_param->type = charArrayType;
    putstring_param->isGlobal = false;
    putstring_param->localIndex = -1;
    putstring_param->next = NULL;
    add_function("putstring", voidType, 1, &putstring_param, false);
}