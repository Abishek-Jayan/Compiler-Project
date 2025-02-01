CC = gcc
CFLAGS = -Wall -Wextra -pedantic
TARGET = mycc

all: $(TARGET)

$(TARGET): main.c
	$(CC) $(CFLAGS) main.c -o $(TARGET)

clean:
	rm -f $(TARGET)