CC      = gcc
CFLAGS  = -Wall -Wextra -g
VERSION = 1.1.0
SRC     = src/ls-v$(VERSION).c
OBJ     = obj/ls-v$(VERSION).o
TARGET  = bin/ls

all: $(TARGET)

$(TARGET): $(OBJ) | bin
	$(CC) $(OBJ) -o $@

obj/%.o: src/%.c | obj
	$(CC) $(CFLAGS) -c $< -o $@

bin obj:
	mkdir -p $@

clean:
	rm -f obj/*.o bin/ls

.PHONY: all clean
