PROGRAM_NAME = elmo

CC = gcc
CFLAGS = -Iinclude -Wall -Wextra -std=c99

SRC = $(wildcard src/*.c)
OBJ = $(SRC:.c=.o)

$(PROGRAM_NAME): $(OBJ)
	$(CC) $(OBJ) -o $(PROGRAM_NAME)
	
src/%.o: src/%.c
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	$(RM) src/*.o $(PROGRAM_NAME)
	
run: ${PROGRAM_NAME}
	./${PROGRAM_NAME} ${ARGS}
