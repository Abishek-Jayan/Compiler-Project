.class public INPUTS/series
.super java/lang/Object

.field public static t F
.field public static s F

.method public static add_term : (I)V
    .code stack 4 locals 3
    ; expression statement at INPUTS/series.c line 7
    iload 0 ; i
    i2f ; cast int to float
    dup
    fstore 2 ; fi
    ; expression statement at INPUTS/series.c line 8
    getstatic Field INPUTS/series t F
    ldc 2.0f
    fload 2 ; fi
    fmul
    ldc 1.0f
    fsub
    fmul
    dup
    putstatic Field INPUTS/series t F
    ; expression statement at INPUTS/series.c line 9
    getstatic Field INPUTS/series t F
    ldc 8.0f
    fload 2 ; fi
    fmul
    fdiv
    dup
    putstatic Field INPUTS/series t F
    ; expression statement at INPUTS/series.c line 11
    getstatic Field INPUTS/series t F
    ldc 2.0f
    fload 2 ; fi
    fmul
    ldc 1.0f
    fadd
    fdiv
    dup
    fstore 1 ; delta
    ; expression statement at INPUTS/series.c line 12
    getstatic Field INPUTS/series s F
    fload 1 ; delta
    fadd
    dup
    putstatic Field INPUTS/series s F
    ; expression statement at INPUTS/series.c line 14
    getstatic Field INPUTS/series s F
    invokestatic Method lib440 putfloat (F)V
    ; expression statement at INPUTS/series.c line 15
    bipush 10
    invokestatic Method lib440 putchar (I)I
    pop
    return
    .end code
.end method

.method public static main : ()I
    .code stack 4 locals 0
    ; expression statement at INPUTS/series.c line 20
    ldc 3.0f
    putstatic Field INPUTS/series t F
    putstatic Field INPUTS/series s F
    ; expression statement at INPUTS/series.c line 22
    iconst_1
    invokestatic Method INPUTS/series add_term (I)V
    ; expression statement at INPUTS/series.c line 23
    iconst_2
    invokestatic Method INPUTS/series add_term (I)V
    ; expression statement at INPUTS/series.c line 24
    iconst_3
    invokestatic Method INPUTS/series add_term (I)V
    ; expression statement at INPUTS/series.c line 25
    iconst_4
    invokestatic Method INPUTS/series add_term (I)V
    ; expression statement at INPUTS/series.c line 26
    iconst_5
    invokestatic Method INPUTS/series add_term (I)V
    ; expression statement at INPUTS/series.c line 27
    bipush 6
    invokestatic Method INPUTS/series add_term (I)V
    ; expression statement at INPUTS/series.c line 28
    bipush 7
    invokestatic Method INPUTS/series add_term (I)V
    ; expression statement at INPUTS/series.c line 29
    bipush 8
    invokestatic Method INPUTS/series add_term (I)V
    ; return statement at INPUTS/series.c line 30
    iconst_0
    ireturn
    .end code
.end method

.method public static main : ([Ljava/lang/String;)V
    .code stack 1 locals 1
    invokestatic Method INPUTS/series main ()I
    invokestatic Method java/lang/System exit (I)V
    return
    .end code
.end method

.method <init> : ()V
    .code stack 1 locals 1
    aload_0
    invokespecial Method java/lang/Object <init> ()V
    return
    .end code
.end method

