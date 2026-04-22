#include <termios.h>
#include <time.h>

#include "abuf.h"
#include "bufchn.h"

#define Bool int

typedef enum MODE
{
	SAFE,
	EDIT
} MODE;

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

typedef struct EDITOR
{
	int cursor_x;
	int cursor_y;
	int cursor_rx;
	int cursor_x_snap;
	int screen_rows;
	int screen_cols;
	int row_offset;
	int col_offset;
	Bool dirty;
	MODE mode;
	BUFFER_CHAIN *buf_chain;
	char *filepath;
	char status_msg[80];
	time_t status_msg_time;
	struct termios og_terminal_conf;
} EDITOR;

void editor_set_status_msg(const char *fmt, ...);
int editor_get_window_size(int *rows, int *cols);
void editor_open(const char *filepath);
void editor_save();
void editor_draw_buffer(APPEND_BUFFER *ab);
void editor_draw_status_bar(APPEND_BUFFER *ab);
void editor_scroll();
void editor_refresh_screen();
int editor_read_key();
void editor_move_cursor(int c);
void editor_insert(int c);
void editor_process_keypress();
int editor_get_cursor_position(int *rows, int *cols);
void init_editor();