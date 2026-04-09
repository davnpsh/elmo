#include <sys/ioctl.h>
#include <unistd.h>
#include <errno.h>
#include <stdlib.h>

#include "editor.h"
#include "helper.h"

#define CTRL_KEY(k) ((k) & 0x1f)

EDITOR editor;

int editor_get_window_size(int *rows, int *cols)
{
	struct winsize ws;
	
	if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &ws) == -1 || ws.ws_col == 0) return -1;
	else
	{
		*cols = ws.ws_col;
	    *rows = ws.ws_row;
		return 0;
	}
}

void editor_refresh_screen()
{
	write(STDOUT_FILENO, "\x1b[2J", 4);
	write(STDOUT_FILENO, "\x1b[H", 3);
	
	// In case of drawing things like welcome text...
	
	write(STDOUT_FILENO, "\x1b[H", 3);
}

char editor_read_key()
{
	int nread;
	char c;
	
	while ((nread = read(STDIN_FILENO, &c, 1)) != 1)
	{
		if (nread == -1 && errno != EAGAIN) 
			die("read");
	}
	
	return c;
}

void editor_process_keypress()
{
	char c = editor_read_key();
	
	switch (c)
	{
		case CTRL_KEY('q'):
			write(STDOUT_FILENO, "\x1b[2J", 4);
			write(STDOUT_FILENO, "\x1b[H", 3);
			exit(0);
			break;
	}
}

void init_editor() 
{
	if (editor_get_window_size(&editor.screen_rows, &editor.screen_cols) == -1) 
		die("editor_get_window_size");
}