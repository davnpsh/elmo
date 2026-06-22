#ifndef EDITOR_H
#define EDITOR_H

#include <termios.h>
#include <time.h>

typedef struct APPEND_BUFFER APPEND_BUFFER;
typedef struct BUFFER_CHAIN BUFFER_CHAIN;

typedef int Bool;
#define TRUE 1
#define FALSE 0

typedef enum EDITOR_MODE
{
	SAFE,
	EDIT
} EDITOR_MODE;

typedef enum LINE_NUMBER_MODE
{
	ABSOLUTE,
	RELATIVE
} LINE_NUMBER_MODE;

enum MOV_KEY
{
	BACKSPACE = 127,
	UP = 1000,
	DOWN,
	LEFT,
	RIGHT,
	PAGE_UP,
	PAGE_DOWN,
	HOME_KEY,
	END_KEY,
	DEL_KEY,
	CTRL_UP,
	CTRL_DOWN,
	CTRL_LEFT,
	CTRL_RIGHT,
	SHIFT_UP,
	SHIFT_DOWN,
	SHIFT_LEFT,
	SHIFT_RIGHT
};

typedef struct POSITION
{
	int x;
	int y;
} POSITION;

typedef struct EDITOR
{
	// Real coordinates
	POSITION cursor;

	// Render coordinates
	// (tabs, split lines)
	POSITION cursor_render;

	// Sticky cursor in X
	// (based on render coordinates)
	int sticky_col;
	Bool sticky_col_update;

	// Prompt
	Bool in_prompt;
	int cursor_px;

	// Offset in total display rows
	int render_offset;

	// Select text
	Bool text_selected;
	POSITION select_anchor;
	POSITION select_start;
	POSITION select_end;
	POSITION r_select_start;	// render
	POSITION r_select_end;		// render

	Bool welcome;
	int screen_rows;
	int screen_cols;
	Bool dirty;
	Bool highlight_current_line;
	Bool show_line_num_gutter;
	LINE_NUMBER_MODE line_num_mode;
	int line_num_gutter_width;
	EDITOR_MODE mode;
	BUFFER_CHAIN *buf_chain;
	char *filepath;
	char status_msg[80];
	time_t status_msg_time;
	struct termios og_terminal_conf;
} EDITOR;

#define CTRL_KEY(k) ((k) & 0x1f)
#define RESERVED_ROWS 2
#define MIN_ROWS_FOR_WELCOME 20
#define MIN_COLS_FOR_WELCOME 50

extern EDITOR editor;

void editor_cleanup();
void editor_set_status_msg(char *fmt, ...);
int editor_set_window_size();
void editor_handle_window_resize(int sig);
void editor_open(char *filepath);
void editor_save();
int editor_get_editable_area_width();
void editor_scroll();
void editor_draw_line_number(APPEND_BUFFER *ab, int *number, int offset);
void editor_draw_buffer(APPEND_BUFFER *ab);
void editor_draw_status_bar(APPEND_BUFFER *ab);
void editor_draw_message_bar(APPEND_BUFFER *ab);
void editor_draw_welcome(APPEND_BUFFER *ab);
void editor_refresh_screen();
int editor_read_key();
void editor_move_cursor(int c);
void editor_move_by_word(int c);
void editor_select(int c);
void editor_insert(int c);
void editor_delete();
void editor_jump(int shift);
void editor_toggle_comment();
void editor_prompt(char *command);
void editor_process_keypress();
void init_editor();

#endif	/* EDITOR_H */