#include <sys/ioctl.h>
#include <unistd.h>
#include <errno.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "editor.h"
#include "helper.h"
#include "abuf.h"

#define CTRL_KEY(k) ((k) & 0x1f)

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
	
	char buf[32];
	snprintf(buf, sizeof(buf), "\x1b[%d;%dH", editor.cursor_y + 1, editor.cursor_x + 1);
	abAppend(&ab, buf, strlen(buf));
	abAppend(&ab, "\x1b[?25h", 6);
	
	write(STDOUT_FILENO, ab.b, ab.len);
	abFree(&ab);
}

int editor_read_key()
{
	int nread;
	char c;
	
	while ((nread = read(STDIN_FILENO, &c, 1)) != 1)
	{
		if (nread == -1 && errno != EAGAIN) 
			die("read");
	}
	
	if (c == '\x1b')
	{
	 	char seq[3];
			
	    if (read(STDIN_FILENO, &seq[0], 1) != 1) return '\x1b';
	    if (read(STDIN_FILENO, &seq[1], 1) != 1) return '\x1b';
		
	    if (seq[0] == '[') 
		{
			if (seq[1] >= '0' && seq[1] <= '9')
			{
				if (read(STDIN_FILENO, &seq[2], 1) != 1) return '\x1b';
				
				if (seq[2] == '~')
			 	{
          			switch (seq[1]) 
             		{
			            case '1': return HOME_KEY;
						case '3': return DEL_KEY;
						case '4': return END_KEY;
               			case '5': return PAGE_UP;
                  		case '6': return PAGE_DOWN;
                   		case '7': return HOME_KEY;
                        case '8': return END_KEY;
               		}
        		}
			}
			else
			{
				switch (seq[1]) 
				{
			      	case 'A': return UP;
			       	case 'B': return DOWN;
				    case 'C': return RIGHT;
				    case 'D': return LEFT;
			        case 'H': return HOME_KEY;
			        case 'F': return END_KEY;
			    }
			}
	    }
		else if (seq[0] == 'O')
		{
			switch (seq[1])
			{
				case 'H': return HOME_KEY;
				case 'F': return END_KEY;
			}
		}
					
		return '\x1b';
	}
	
	return c;
}

void editor_move_cursor(int c)
{
	switch (c)
	{
		case UP:
			if (editor.cursor_y != 0)
				editor.cursor_y--;
			break;
		case DOWN:
			if (editor.cursor_y != editor.screen_rows - 1)
				editor.cursor_y++;
			break;
		case LEFT:
			if (editor.cursor_x != 0)
				editor.cursor_x--;
			break;	
		case RIGHT:
			if (editor.cursor_x != editor.screen_cols - 1)
				editor.cursor_x++;
			break;
	}
}

void editor_process_keypress()
{
	int c = editor_read_key();
	
	switch (c)
	{
		case CTRL_KEY('q'):
			write(STDOUT_FILENO, "\x1b[2J", 4);
			write(STDOUT_FILENO, "\x1b[H", 3);
			exit(0);
			break;
			
		case UP:
		case DOWN:
		case LEFT:
		case RIGHT:
			editor_move_cursor(c);
			break;
			
		case PAGE_UP:
		case PAGE_DOWN:
			// Do nothing for now
			break;
			
		case HOME_KEY:
		case END_KEY:
		case DEL_KEY:
			// Do nothing for now
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
	editor.cursor_x = editor.cursor_y = 0;
	
	if (editor_get_window_size(&editor.screen_rows, &editor.screen_cols) == -1) 
		die("editor_get_window_size");
}