#include "codegen.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdarg.h>
#include "parser_ast.h"


static void emit_class_header(CodegenContext *ctx);
static void emit_global_variables(CodegenContext *ctx);
static void emit_functions(CodegenContext *ctx, Statement **stmts, int stmtCount);
static void emit_main_method(CodegenContext *ctx);
static void emit_init_method(CodegenContext *ctx);
static void emit_clinit_method(CodegenContext *ctx);
static void emit_function(CodegenContext *ctx, Function *func);
static void emit_statement(CodegenContext *ctx, Statement *stmt);
static void emit_expression(CodegenContext *ctx, Expression *expr);
static const char *get_jvm_type(Type *type);


static void emit(CodegenContext *ctx, const char *fmt, ...) {
    va_list args;
    va_start(args, fmt);
    fprintf(ctx->output, "    ");
    vfprintf(ctx->output, fmt, args);
    fprintf(ctx->output, "\n");
    va_end(args);
}


static const char *get_jvm_type(Type *type) {
    if (type->isArray) {
        if (type->base == BASE_CHAR) return "[C";
        return "[I";
    }
    switch (type->base) {
        case BASE_INT: return "I";
        case BASE_CHAR: return "C";
        case BASE_FLOAT: return "F";
        case BASE_VOID: return "V";
        case BASE_STRUCT: return "Ljava/lang/Object;"; 
        default: return "V";
    }
}

void generate_code(Statement **stmts, int stmtCount, const char *filename, const char *outfilename) {
    CodegenContext ctx = {0};
    ctx.filename = filename;
    ctx.currentfunc = NULL;
    ctx.localcount = 0;
    ctx.stacksize = 0;
    ctx.maxstacksize = 0;

    ctx.output = fopen(outfilename, "w");
    if (!ctx.output) {
        fprintf(stderr, "Error: Cannot open output file %s\n", outfilename);
        exit(1);
    }

    emit_class_header(&ctx);
    emit_global_variables(&ctx);
    emit_functions(&ctx, stmts, stmtCount);
    emit_main_method(&ctx);
    emit_init_method(&ctx);
    emit_clinit_method(&ctx);

    fclose(ctx.output);
}

static void emit_class_header(CodegenContext *ctx) {
    char class_name[256];
    snprintf(class_name, sizeof(class_name), "%.*s", (int)(strlen(ctx->filename) - 2), ctx->filename);
    fprintf(ctx->output, ".class public %s\n", class_name);
    fprintf(ctx->output, ".super java/lang/Object\n\n");
}

static void emit_global_variables(CodegenContext *ctx) {
    VarSymbol *var = varSymbols;
    while (var) {
        if (var->isGlobal) {
            fprintf(ctx->output, ".field public static %s %s\n", var->name, get_jvm_type(&var->type));
        }
        var = var->next;
    }
    if (varSymbols) fprintf(ctx->output, "\n");
}

static void emit_functions(CodegenContext *ctx, Statement **stmts, int stmtCount) {
    for (int i = 0; i < stmtCount; i++) {
        Statement *stmt = stmts[i];
        if (stmt->kind == STMT_COMPOUND && stmt->u.func) {
            emit_function(ctx, stmt->u.func);
        }
    }
}

static void emit_main_method(CodegenContext *ctx) {
    fprintf(ctx->output, ".method public static main : ([Ljava/lang/String;)V\n");
    emit(ctx, ".code stack 1 locals 1");
    emit(ctx, "invokestatic Method %.*s main ()I", (int)(strlen(ctx->filename) - 2), ctx->filename);
    emit(ctx, "invokestatic Method java/lang/System exit (I)V");
    emit(ctx, "return");
    emit(ctx, ".end code");
    fprintf(ctx->output, ".end method\n\n");
}

static void emit_init_method(CodegenContext *ctx) {
    fprintf(ctx->output, ".method <init> : ()V\n");
    emit(ctx, ".code stack 1 locals 1");
    emit(ctx, "aload_0");
    emit(ctx, "invokespecial Method java/lang/Object <init> ()V");
    emit(ctx, "return");
    emit(ctx, ".end code");
    fprintf(ctx->output, ".end method\n\n");
}

static void emit_clinit_method(CodegenContext *ctx) {
    bool needed = false;
    VarSymbol *var = varSymbols;
    while (var) {
        if (var->isGlobal && (var->type.isArray || /* initialized */ false)) {
            needed = true;
            break;
        }
        var = var->next;
    }
    if (!needed) return;

    fprintf(ctx->output, ".method <clinit> : ()V\n");
    emit(ctx, ".code stack 2 locals 0");
    emit(ctx, "return");
    emit(ctx, ".end code");
    fprintf(ctx->output, ".end method\n\n");
}

static void emit_function(CodegenContext *ctx, Function *func) {
    ctx->currentfunc = func;
    ctx->localcount = func->numParams;
    ctx->stacksize = 0;
    ctx->maxstacksize = 0;

    for (int i = 0; i < func->numParams; i++) {
        VarSymbol *vs = lookup_variable(func->params[i]->name);
        if (vs) vs->localIndex = i;
    }

    VarSymbol *vs = varSymbols;
    while (vs) {
        if (!vs->isGlobal && vs->localIndex == -1) {
            vs->localIndex = ctx->localcount++;
        }
        vs = vs->next;
    }

    char signature[256] = "(";
    for (int i = 0; i < func->numParams; i++) {
        strcat(signature, get_jvm_type(&func->params[i]->declType));
    }
    strcat(signature, ")");
    strcat(signature, get_jvm_type(&func->returnType));

    fprintf(ctx->output, ".method public static %s : %s\n", func->name, signature);
    emit(ctx, ".code stack %d locals %d", func->stackLimit ? func->stackLimit : 2, ctx->localcount);

    emit_statement(ctx, func->body);

    if (func->returnType.base == BASE_VOID) {
        emit(ctx, "return");
    }

    emit(ctx, ".end code");
    fprintf(ctx->output, ".end method\n\n");
}

static void emit_statement(CodegenContext *ctx, Statement *stmt) {
    static bool inDeadCode = false;
    if (!stmt) return;

    if (stmt->kind == STMT_RETURN) {
        inDeadCode = true;
        emit(ctx, "; return statement at %s line %d", ctx->filename, stmt->lineno);
        if (stmt->u.expr) {
            emit_expression(ctx, stmt->u.expr);
            emit(ctx, "%sreturn", stmt->u.expr->exprType.base == BASE_FLOAT ? "f" : "i");
        } else {
            emit(ctx, "return");
        }
        return;
    }

    if (inDeadCode) {
        emit(ctx, "DEAD ; statement at %s line %d", ctx->filename, stmt->lineno);
        return;
    }

    switch (stmt->kind) {
        case STMT_EXPR:
            if (stmt->u.expr) {
                emit(ctx, "; expression statement at %s line %d", ctx->filename, stmt->lineno);
                emit_expression(ctx, stmt->u.expr);
                if (stmt->u.expr->exprType.base != BASE_VOID) {
                    emit(ctx, "pop");
                }
            }
            break;
        case STMT_RETURN:
            break;
        case STMT_COMPOUND:
            for (int i = 0; i < stmt->numStmts; i++) {
                emit_statement(ctx, stmt->stmts[i]);
            }
            break;
        case STMT_DECL:
            if (stmt->u.decl.initialized && stmt->u.decl.init) {
                emit(ctx, "; declaration initialization at %s line %d", ctx->filename, stmt->lineno);
                emit_expression(ctx, stmt->u.decl.init);
                VarSymbol *vs = lookup_variable(stmt->u.decl.name);
                if (vs && !vs->isGlobal) {
                    emit(ctx, "%sstore %d ; %s",
                         vs->type.base == BASE_FLOAT ? "f" : "i",
                         vs->localIndex, vs->name);
                }
            }
            break;
    }
}

static void emit_expression(CodegenContext *ctx, Expression *expr) {
    if (!expr) return;
    ctx->stacksize++;
    if (ctx->stacksize > ctx->maxstacksize) {
        ctx->maxstacksize = ctx->stacksize;
        ctx->currentfunc->stackLimit = ctx->maxstacksize;
    }

    switch (expr->kind) {
        case EXPR_LITERAL:
            if (expr->exprType.base == BASE_INT) {
                int val = atoi(expr->value);
                if (val >= -1 && val <= 5) {
                    emit(ctx, "iconst_%d", val);
                } else if (val >= -128 && val <= 127) {
                    emit(ctx, "bipush %d", val);
                } else {
                    emit(ctx, "ldc %d", val);
                }
            } else if (expr->exprType.base == BASE_CHAR) {
                emit(ctx, "bipush %d", expr->value[1]); // Handle 'c'
            } else if (expr->exprType.base == BASE_FLOAT) {
                emit(ctx, "ldc %s", expr->value);
            } else if (expr->exprType.isArray && expr->exprType.base == BASE_CHAR) {
                emit(ctx, "ldc \"%s\"", expr->value);
                emit(ctx, "invokestatic Method lib440 java2c (Ljava/lang/String;)[C");
            }
            break;
        case EXPR_IDENTIFIER:
            {
                VarSymbol *vs = lookup_variable(expr->value);
                if (!vs) {
                    fprintf(stderr, "Code generation error in file %s line %d: Undeclared variable %s\n",
                            ctx->filename, expr->lineno, expr->value);
                    exit(1);
                }
                if (vs->isGlobal) {
                    emit(ctx, "getstatic Field %.*s %s %s",
                         (int)(strlen(ctx->filename) - 2), ctx->filename,
                         vs->name, get_jvm_type(&vs->type));
                } else {
                    emit(ctx, "%sload %d ; %s",
                         vs->type.base == BASE_FLOAT ? "f" : "i",
                         vs->localIndex, vs->name);
                }
            }
            break;
        case EXPR_BINARY:
            emit_expression(ctx, expr->left);
            emit_expression(ctx, expr->right);
            ctx->stacksize--; // Two operands consumed, one result produced
            if (expr->exprType.base == BASE_INT) {
                switch (expr->op) {
                    case OP_PLUS: emit(ctx, "iadd"); break;
                    case OP_MINUS: emit(ctx, "isub"); break;
                    case OP_MUL: emit(ctx, "imul"); break;
                    case OP_DIV: emit(ctx, "idiv"); break;
                    case OP_MOD: emit(ctx, "irem"); break;
                    default:
                        fprintf(stderr, "Code generation error in file %s line %d: Unsupported binary operator\n",
                                ctx->filename, expr->lineno);
                        exit(1);
                }
            } else if (expr->exprType.base == BASE_FLOAT) {
                switch (expr->op) {
                    case OP_PLUS: emit(ctx, "fadd"); break;
                    case OP_MINUS: emit(ctx, "fsub"); break;
                    case OP_MUL: emit(ctx, "fmul"); break;
                    case OP_DIV: emit(ctx, "fdiv"); break;
                    default:
                        fprintf(stderr, "Code generation error in file %s line %d: Unsupported float operator\n",
                                ctx->filename, expr->lineno);
                        exit(1);
                }
            }
            break;
        case EXPR_ASSIGN:
            emit_expression(ctx, expr->right);
            VarSymbol *vs = lookup_variable(expr->left->value);
            if (!vs) {
                fprintf(stderr, "Code generation error in file %s line %d: Undeclared variable %s\n",
                        ctx->filename, expr->lineno, expr->left->value);
                exit(1);
            }
            emit(ctx,"dup");
            if (vs->isGlobal) {
                emit(ctx, "putstatic Field %.*s %s %s",
                     (int)(strlen(ctx->filename) - 2), ctx->filename,
                     vs->name, get_jvm_type(&vs->type));
            } else {
                emit(ctx, "%sstore %d ; %s",
                     vs->type.base == BASE_FLOAT ? "f" : "i",
                     vs->localIndex, vs->name);
            }
            break;
        case EXPR_CALL:
            {
                for (int i = 0; i < expr->numArgs; i++) {
                    emit_expression(ctx, expr->args[i]);
                }
                ctx->stacksize -= expr->numArgs;
                Function *func = lookup_function(expr->left->value);
                if (!func) {
                    // Check if it's a lib440 function
                    if (strcmp(expr->left->value, "putstring") == 0 ||
                        strcmp(expr->left->value, "putint") == 0 ||
                        strcmp(expr->left->value, "putchar") == 0 ||
                        strcmp(expr->left->value, "getint") == 0 ||
                        strcmp(expr->left->value, "getchar") == 0 ||
                        strcmp(expr->left->value, "putfloat") == 0 ||
                        strcmp(expr->left->value, "getfloat") == 0) {
                        char signature[256] = "(";
                        for (int i = 0; i < expr->numArgs; i++) {
                            strcat(signature, get_jvm_type(&expr->args[i]->exprType));
                        }
                        strcat(signature, ")");
                        strcat(signature, get_jvm_type(&expr->exprType));
                        emit(ctx, "invokestatic Method lib440 %s %s",
                             expr->left->value, signature);
                    } else {
                        fprintf(stderr, "Code generation error in file %s line %d: Undeclared function %s\n",
                                ctx->filename, expr->lineno, expr->left->value);
                        exit(1);
                    }
                } else {
                    char signature[256] = "(";
                    for (int i = 0; i < func->numParams; i++) {
                        strcat(signature, get_jvm_type(&func->params[i]->declType));
                    }
                    strcat(signature, ")");
                    strcat(signature, get_jvm_type(&func->returnType));
                    emit(ctx, "invokestatic Method %.*s %s %s",
                         (int)(strlen(ctx->filename) - 2), ctx->filename,
                         func->name, signature);
                }
            }
            break;
        case EXPR_UNARY:
            emit_expression(ctx, expr->right);
            if (expr->op == OP_NEG) {
                emit(ctx, "%sneg", expr->exprType.base == BASE_FLOAT ? "f" : "i");
            } else if (expr->op == OP_INC || expr->op == OP_DEC) {
                VarSymbol *vs = lookup_variable(expr->right->value);
                if (!vs) {
                    fprintf(stderr, "Code generation error in file %s line %d: Undeclared variable %s\n",
                            ctx->filename, expr->lineno, expr->right->value);
                    exit(1);
                }
                if (vs->isGlobal) {
                    emit(ctx, "getstatic Field %.*s %s %s",
                         (int)(strlen(ctx->filename) - 2), ctx->filename,
                         vs->name, get_jvm_type(&vs->type));
                    emit(ctx, "iconst_1");
                    emit(ctx, expr->op == OP_INC ? "iadd" : "isub");
                    emit(ctx, "putstatic Field %.*s %s %s",
                         (int)(strlen(ctx->filename) - 2), ctx->filename,
                         vs->name, get_jvm_type(&vs->type));
                } else {
                    emit(ctx, "iinc %d %d ; %s",
                         vs->localIndex, expr->op == OP_INC ? 1 : -1, vs->name);
                }
                emit(ctx, "%sload %d ; %s",
                     vs->type.base == BASE_FLOAT ? "f" : "i",
                     vs->localIndex, vs->name);
            }
            break;
        case EXPR_CAST:
            // Handle cast by emitting code for the subexpression
            emit_expression(ctx, expr->left);
            // For char to int casts, no additional instructions needed (JVM type 'I')
            if (!(expr->exprType.base == BASE_INT && expr->left->exprType.base == BASE_CHAR)) {
                fprintf(stderr, "Code generation error in file %s line %d: Unsupported cast type\n",
                        ctx->filename, expr->lineno);
                exit(1);
            }
            break;
        default:
            fprintf(stderr, "Code generation error in file %s line %d: Unsupported expression type\n",
                    ctx->filename, expr->lineno);
            exit(1);
    }
}