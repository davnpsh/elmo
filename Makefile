PROGRAM_NAME = elmo
CC = gcc
CFLAGS = -Iinclude -Wall -Wextra -std=c99
SRC = $(wildcard src/*.c)
OBJ = $(patsubst src/%.c, build/%.o, $(SRC))

$(PROGRAM_NAME): $(OBJ)
	$(CC) $(OBJ) -o $(PROGRAM_NAME)

build/%.o: src/%.c | build
	$(CC) $(CFLAGS) -c $< -o $@

build:
	mkdir -p build

clean:
	$(RM) -r build $(PROGRAM_NAME)

run: $(PROGRAM_NAME)
	./$(PROGRAM_NAME) $(ARGS)