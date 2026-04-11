#include <sys/ioctl.h>
#include <unistd.h>
#include <errno.h>
#include <stdlib.h>
#include <stdio.h>

#include "editor.h"
#include "helper.h"
#include "abuf.h"

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
	ABUF ab = {NULL, 0};
	
	abAppend(&ab, "\x1b[?25l", 6);
	abAppend(&ab, "\x1b[2J", 4);
	abAppend(&ab, "\x1b[H", 3);
	
	// ---
	// In case of drawing things like welcome text
	// it goes here
	// ---
	
	abAppend(&ab, "\x1b[H", 3);
	abAppend(&ab, "\x1b[?25h", 6);
	
	write(STDOUT_FILENO, ab.b, ab.len);
	abFree(&ab);
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

int editor_get_cursor_position(int *rows, int *cols)
{
	char buf[32];
	unsigned int i = 0;
	
	if (write(STDOUT_FILENO, "\x1b[6n", 4) != 4) return -1;
	
	while (i < sizeof(buf) - 1) {
		if (read(STDIN_FILENO, &buf[i], 1) != 1) break;
		if (buf[i] == 'R') break;
		i++;
	}
	buf[i] = '\0';
	
	if (buf[0] != '\x1b' || buf[1] != '[') return -1;
	if (sscanf(&buf[2], "%d;%d", rows, cols) != 2) return -1;
	
	return 0;
}

void init_editor() 
{
	if (editor_get_window_size(&editor.screen_rows, &editor.screen_cols) == -1) 
		die("editor_get_window_size");
}