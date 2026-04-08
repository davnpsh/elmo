PROGRAM_NAME = elmo

$(PROGRAM_NAME): $(PROGRAM_NAME).c
	$(CC) $(PROGRAM_NAME).c -o $(PROGRAM_NAME) -Wall -Wextra -pedantic -std=c99

clean:
	$(RM) $(PROGRAM_NAME)
	
run: ${PROGRAM_NAME}
	./${PROGRAM_NAME} ${ARGS}
	