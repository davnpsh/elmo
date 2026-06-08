#include <termios.h>
#include <time.h>

#include "bufchn.h"

typedef struct APPEND_BUFFER APPEND_BUFFER;

typedef enum MODE
{
	SAFE,
	EDIT
} MODE;

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
	DEL_KEY
};

enum CTRL_KEYS
{
	CTRL_UP = 2000,
	CTRL_DOWN,
	CTRL_LEFT,
	CTRL_RIGHT
};

enum SHIFT_KEYS
{
	SHIFT_UP = 3000,
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
	int cursor_x;
	int cursor_y;

	// Render coordinates
	// (tabs, split lines)
	int cursor_rx;
	int cursor_ry;

	// Sticky cursor in X
	// (based on render coordinates)
	int sticky_col;
	Bool sticky_col_update;

	// Cursor position in prompt
	int cursor_px;

	// Offset in total display rows
	int render_offset;

	// Select text
	Bool text_selected;
	POSITION select_start;
	POSITION select_end;
	POSITION r_select_start;	// render
	POSITION r_select_end;		// render

	int screen_rows;
	int screen_cols;
	Bool dirty;
	Bool highlight_current_line;
	Bool show_line_num_gutter;
	LINE_NUMBER_MODE line_num_mode;
	int line_num_gutter_width;
	MODE mode;
	BUFFER_CHAIN *buf_chain;
	char *filepath;
	char status_msg[80];
	time_t status_msg_time;
	struct termios og_terminal_conf;
} EDITOR;

void editor_set_status_msg(const char *fmt, ...);
int editor_set_window_size();
void editor_open(char *filepath);
void editor_save();
void editor_draw_buffer(APPEND_BUFFER *ab);
void editor_draw_status_bar(APPEND_BUFFER *ab);
void editor_draw_welcome(APPEND_BUFFER *ab);
void editor_scroll();
void editor_refresh_screen(Bool in_prompt);
int editor_read_key();
void editor_move_cursor(int c);
void editor_move_word(int c);
void editor_insert(int c);
void editor_delete();
void editor_process_command(char* command);
void editor_prompt(const char *command);
void editor_process_keypress();
int editor_get_cursor_position(int *rows, int *cols);
void init_editor();