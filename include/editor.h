#include <termios.h>

typedef struct EDITOR
{
	int cursor_x, cursor_y;
	int screen_rows, screen_cols;
	struct termios og_terminal_conf;
} EDITOR;

int editor_get_window_size(int *rows, int *cols);
void editor_refresh_screen();
int editor_read_key();
void editor_move_cursor(int c);
void editor_process_keypress();
int editor_get_cursor_position(int *rows, int *cols);
void init_editor();