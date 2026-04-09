#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <ctype.h>
#include <errno.h>
#include <string.h>

#include "helper.h"
#include "editor.h"

extern EDITOR editor;

void disable_raw_mode()
{
	if (tcsetattr(STDIN_FILENO, TCSAFLUSH, &editor.og_terminal_conf) == -1) 
		die("tcsetattr");
}

void enable_raw_mode() 
{
	if (tcgetattr(STDIN_FILENO, &editor.og_terminal_conf) == -1) 
		die("tcgetattr");
	
	atexit(disable_raw_mode);
	
	struct termios raw = editor.og_terminal_conf;
	
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