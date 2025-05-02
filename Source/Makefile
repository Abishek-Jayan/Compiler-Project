CC = gcc
CFLAGS = -Wall -Wextra -pedantic
TARGET = mycc

SRCS = main.c lexer.c parser.c parser_ast.c codegen.c

OBJS = $(SRCS:.c=.o)
OUTPUT = *.parser *.lexer *.types *.j

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CC) $(CFLAGS) $(OBJS) -o $(TARGET)

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -f $(TARGET) $(OBJS) $(OUTPUT)