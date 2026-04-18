#include <termios.h>

#include "abuf.h"
#include "bufchn.h"

typedef struct EDITOR
{
	int cursor_x, cursor_y;
	int screen_rows, screen_cols;
	struct termios og_terminal_conf;
	BUFFER_CHAIN *buf_chain;
} EDITOR;

int editor_get_window_size(int *rows, int *cols);
void editor_open(const char *file_path);
void editor_draw(APPEND_BUFFER *ab);
void editor_refresh_screen();
int editor_read_key();
void editor_move_cursor(int c);
void editor_process_keypress();
int editor_get_cursor_position(int *rows, int *cols);
void init_editor();