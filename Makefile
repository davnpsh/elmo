PROGRAM_NAME = elmo
CC = gcc
CFLAGS = -Iinclude -Wall -Wextra -std=c99 -g -O0
SRC = $(wildcard src/*.c)
OBJ = $(patsubst src/%.c, build/%.o, $(SRC))

$(PROGRAM_NAME): $(OBJ)
	$(CC) $(OBJ) -o $(PROGRAM_NAME)

build/%.o: src/%.c | build
	$(CC) $(CFLAGS) -O2 -c $< -o $@

debug: CFLAGS += -O0 -g -fsanitize=address,undefined
debug: $(OBJ)
	$(CC) $(OBJ) -fsanitize=address,undefined -o $(PROGRAM_NAME)

build:
	mkdir -p build

clean:
	$(RM) -r build $(PROGRAM_NAME)