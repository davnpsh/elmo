#include <termios.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <ctype.h>
#include <errno.h>
#include <sys/ioctl.h>
#include <string.h>
#include "editor.h"

#define CTRL_KEY(k) ((k) & 0x1f)
#define ABUF_INIT {NULL, 0}

EDITOR_CONFIG Editor;

struct abuf 
{
	char *b;
	int len;
};

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
 
void reposition_cursor()
{
	write(STDOUT_FILENO, "\x1b[H", 3);
}

void editor_draw_rows()
{
	// Just draw 1 line for now
	write(STDOUT_FILENO, "~\r\n", 3);
}

void die(const char *s) 
{
	write(STDOUT_FILENO, "\x1b[2J", 4);
	reposition_cursor();
	perror(s);
	exit(1);
}

void init_editor() {
  if (editor_get_window_size(&Editor.screenrows, &Editor.screencols) == -1) die("editor_get_window_size");
}

void disable_raw_mode()
{
	if (tcsetattr(STDIN_FILENO, TCSAFLUSH, &Editor.orig_termios) == -1) 
		die("tcsetattr");
}

char editor_read_key()
{
	int nread;
	char c;
	
	while ((nread = read(STDIN_FILENO, &c, 1)) != 1)
	{
		if (nread == -1 && errno != EAGAIN) die("read");
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
			reposition_cursor();
			exit(0);
			break;
	}
}

void editor_refresh_screen()
{
	write(STDOUT_FILENO, "\x1b[2J", 4);
	reposition_cursor();
	editor_draw_rows();
	reposition_cursor();
}

void enable_raw_mode() 
{
	if (tcgetattr(STDIN_FILENO, &Editor.orig_termios) == -1) 
		die("tcgetattr");
	
	atexit(disable_raw_mode);
	
	struct termios raw = Editor.orig_termios;
	
	raw.c_iflag &= ~(BRKINT | ICRNL | INPCK | ISTRIP | IXON);
	raw.c_oflag &= ~(OPOST);
	raw.c_cflag |= (CS8);
	raw.c_lflag &= ~(ECHO | ICANON | IEXTEN | ISIG);
	raw.c_cc[VMIN] = 0;
	raw.c_cc[VTIME] = 1;
	
	if (tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw) == -1) 
		die("tcsetattr");
}

int main() 
{
	enable_raw_mode();
	init_editor();
	
	while (1) 
	{
		editor_refresh_screen();
		editor_process_keypress();
	}
	
	return 0;
}