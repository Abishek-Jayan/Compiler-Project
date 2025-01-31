CC = gcc
CFLAGS = -Wall -Wextra -pedantic
TARGET = mycc

all: $(TARGET)

$(TARGET): mycc.c
	$(CC) $(CFLAGS) mycc.c -o $(TARGET)

clean:
	rm -f $(TARGET)