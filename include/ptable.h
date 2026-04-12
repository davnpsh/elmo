#include <stdlib.h>
#include <stdio.h>

#include "helper.h"

typedef enum SOURCE
{
	ORIGINAL,
	ADDED
} SOURCE;

typedef struct PIECE
{
	SOURCE source;
	int start;
	int len;
	int *prev;
	int *next;
} PIECE;

typedef struct PIECE_TABLE
{
	char *original_buffer;
	int original_buffer_size;
	char *new_buffer;
	int new_buffer_size;
	PIECE *piece_ptr;
} PIECE_TABLE; 

PIECE_TABLE *init_piece_table(const char *restrict path)
{
	PIECE_TABLE *piece_table = malloc(sizeof(PIECE_TABLE));
	
	FILE *file = fopen(path, "r");
	if (!file) die("fopen");
	
	// Load original buffer into memory
	fseek(file, 0L, SEEK_END);
	long original_buffer_size = ftell(file);
	piece_table->original_buffer = malloc(sizeof(char) * (original_buffer_size + 1));
	fseek(file, 0L, SEEK_SET);
	size_t new_len = fread(piece_table->original_buffer, sizeof(char), original_buffer_size, file);
	piece_table->original_buffer[new_len++] = '\0';
	piece_table->original_buffer_size = new_len;
	
	fclose(file);
	
	// Initialize new buffer
	piece_table->new_buffer = malloc(sizeof(char));
	piece_table->new_buffer[0] = '\0';
	piece_table->new_buffer_size = 1;
	
	// Initialize Pieces linked list
	piece_table->piece_ptr = malloc(sizeof(PIECE));
	piece_table->piece_ptr.source = ORIGINAL;
	piece_table->piece_ptr.start = 0;
	piece_table->piece_ptr.len = new_len;
	piece_table->piece_ptr.prev = NULL;
	piece_table->piece_ptr.next = NULL;
	
	return piece_table;
}