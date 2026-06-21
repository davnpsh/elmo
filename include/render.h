/*
 * This file contains helper functions
 * for rendering text in the editor
 */

#ifndef RENDER_H
#define RENDER_H

typedef struct BUFFER_CHAIN BUFFER_CHAIN;
typedef struct POSITION POSITION;

typedef enum CHAR_TYPE
{
	WHITESPACE,
	WORD,
	PUNCTUATION
} CHAR_TYPE;

typedef int Bool;
#define TRUE 1
#define FALSE 0

#define TAB_STOP 4

int rx_to_cx(char *s, int len, int cursor_rx);
int cx_to_rx(char *s, int cursor_x);
int get_line_wrap_rows(int line_len, int width);
void update_layout(BUFFER_CHAIN *buf_chain, int width);
void render_coords(POSITION *cursor_render, POSITION cursor, BUFFER_CHAIN *buf_chain);
void get_offset_coordinates(int *row_offset, int *wrap_offset, int ry, BUFFER_CHAIN *buf_chain);
int token_to_color(unsigned char token);
CHAR_TYPE get_char_type(char c);
int digit_count(int n);

#endif /* RENDER_H */