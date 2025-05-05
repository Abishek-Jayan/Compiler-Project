.class public INPUTS/hello
.super java/lang/Object


.method public static main : ()I
    .code stack 2 locals 0
    ; expression statement at INPUTS/hello.c line 4
    bipush 72
    invokestatic Method lib440 putstring ([C)V
    ; return statement at INPUTS/hello.c line 5
    iconst_2
    ireturn
    ; expression statement at INPUTS/hello.c line 6
    bipush 84
    invokestatic Method lib440 putstring ([C)V
    ; return statement at INPUTS/hello.c line 7
    iconst_0
    ireturn
    .end code
.end method

.method public static main : ([Ljava/lang/String;)V
    .code stack 1 locals 1
    invokestatic Method INPUTS/hello main ()I
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

