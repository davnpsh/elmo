/*
 * This file contains helper functions
 * for rendering text in the editor
 */

typedef struct BUFFER_CHAIN BUFFER_CHAIN;

typedef enum CHAR_TYPE
{
	WHITESPACE,
	WORD,
	PUNCTUATION
} CHAR_TYPE;

#define TAB_STOP 4

int rx_to_cx(char *s, int len, int cursor_rx);
int cx_to_rx(char *s, int cursor_x);
int get_line_wrap_rows(int line_len, int width);
void update_layout(BUFFER_CHAIN *buf_chain, int width);
void render_coords(int *rx, int *ry, int x, int y, BUFFER_CHAIN *buf_chain);
void get_offset_coordinates(int *row_offset, int *wrap_offset, int ry, BUFFER_CHAIN *buf_chain);
int token_to_color(unsigned char token);
CHAR_TYPE get_char_type(char c);
int digit_count(int n);