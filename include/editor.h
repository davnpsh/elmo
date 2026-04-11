#include <termios.h>

typedef struct EDITOR
{
	int screen_rows;
	int screen_cols;
	struct termios og_terminal_conf;
} EDITOR;

int editor_get_window_size(int *rows, int *cols);
void editor_refresh_screen();
char editor_read_key();
void editor_process_keypress();
int editor_get_cursor_position(int *rows, int *cols);
void init_editor();