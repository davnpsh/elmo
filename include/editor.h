#include <termios.h>

#include "abuf.h"
#include "bufchn.h"

enum MOV_KEY
{
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
	int screen_rows;
	int screen_cols;
	int row_offset;
	int col_offset;
	BUFFER_CHAIN *buf_chain;
	struct termios og_terminal_conf;
} EDITOR;

int editor_get_window_size(int *rows, int *cols);
void editor_open(const char *file_path);
void editor_draw(APPEND_BUFFER *ab);
void editor_scroll();
void editor_refresh_screen();
int editor_read_key();
void editor_move_cursor(int c);
void editor_process_keypress();
int editor_get_cursor_position(int *rows, int *cols);
void init_editor();