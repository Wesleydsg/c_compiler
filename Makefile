CC = gcc
CFLAGS = -Wall -Wextra -g
TARGET = lexer
SRCS = lexer.c main.c
INPUT = input.txt

$(TARGET): $(SRCS)
	$(CC) $(CFLAGS) -o $(TARGET) $(SRCS)

run: $(TARGET)
	./$(TARGET) $(INPUT)

clean:
	rm -f $(TARGET)