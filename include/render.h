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

typedef enum SELECTION_STATE
{
	SEL_NONE,
    SEL_START,
    SEL_INSIDE,
    SEL_END
} SELECTION_STATE;

typedef int Bool;
#define TRUE 1
#define FALSE 0

#define TAB_STOP 4

int rx_to_cx(char *s, int len, int cursor_rx);
int cx_to_rx(char *s, int cursor_x);
int get_line_wrap_rows(int line_len, int width);
void update_layout(BUFFER_CHAIN *buf_chain, int width);
void render_coords(BUFFER_CHAIN *buf_chain, POSITION cursor, POSITION *cursor_render);
void get_offset_coordinates(BUFFER_CHAIN *buf_chain, int ry, int *row_offset, int *wrap_offset);
Bool is_row_in_selection(Bool text_selected, POSITION render_select_start, POSITION render_select_end, int display_row);
SELECTION_STATE get_selection_state(Bool text_selected, POSITION render_select_start, POSITION render_select_end, int display_row, int ry, Bool in_selection, int width);
int token_to_color(unsigned char token);
CHAR_TYPE get_char_type(char c);
int digit_count(int n);

#endif /* RENDER_H */