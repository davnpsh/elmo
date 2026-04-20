#include <sys/ioctl.h>
#include <unistd.h>
#include <errno.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <libgen.h>
#include <stdarg.h>

#include "editor.h"
#include "helper.h"

#define TRUE 1
#define FALSE 0
#define CTRL_KEY(k) ((k) & 0x1f)

EDITOR editor;

void editor_set_status_msg(const char *fmt, ...) 
{
	va_list ap;
	va_start(ap, fmt);
	vsnprintf(editor.status_msg, sizeof(editor.status_msg), fmt, ap);
	va_end(ap);
	editor.status_msg_time = time(NULL);
}

int editor_get_window_size(int *rows, int *cols)
{
	struct winsize ws;
	
	if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &ws) == -1 || ws.ws_col == 0) return -1;
	
	*cols = ws.ws_col;
    *rows = ws.ws_row;
    
    // Reserve space for status and prompt bar
	*rows -= 2;
	
	return 0;
}

void editor_open(const char *filepath)
{
	editor.filepath = malloc(strlen(filepath) + 1);
	strcpy(editor.filepath, filepath);
	
	editor.buf_chain = buf_parse_file(filepath);
}

void editor_scroll()
{
	editor.cursor_rx = 0;
	
	if (editor.cursor_y < editor.buf_chain->lines_num)
	{
		BUFFER_NODE *buf_node = buf_get_line_at(editor.buf_chain, editor.cursor_y + 1, FALSE);
		
		editor.cursor_rx = cx_to_rx(buf_node->s, editor.cursor_x);
	}
	
	// Scroll up
	if (editor.cursor_y < editor.row_offset)
	{
		editor.row_offset = editor.cursor_y;
	}
	
	// Scroll down
	if (editor.cursor_y >= editor.row_offset + editor.screen_rows)
	{
		editor.row_offset = editor.cursor_y - editor.screen_rows + 1;
	}
	
	// Scroll left
	if (editor.cursor_rx < editor.col_offset)
	{
		editor.col_offset = editor.cursor_rx;
	}
	
	// Scroll right
	if (editor.cursor_rx >= editor.col_offset + editor.screen_cols)
	{
		editor.col_offset = editor.cursor_rx - editor.screen_cols + 1;
	}
}

void editor_draw_buffer(APPEND_BUFFER *ab)
{
	BUFFER_NODE *current_line = buf_get_line_at(editor.buf_chain, 1 + editor.row_offset, TRUE);
	
	for (int y = 0; y < editor.screen_rows; y++)
	{
		if ((y + editor.row_offset) < editor.buf_chain->lines_num)
		{
			int len = current_line->rlen - editor.col_offset;
			if (len < 0) len = 0;
			if (len > editor.screen_cols) len = editor.screen_cols;
			
			ab_append(ab, &current_line->r[editor.col_offset], len);
			
			current_line = current_line->next;
		}
		
		ab_append(ab, "\x1b[K", 3);
		ab_append(ab, "\r\n", 2);
	}
}

void editor_draw_status_bar(APPEND_BUFFER *ab)
{
	ab_append(ab, "\x1b[7m", 4);
	
	char status[100];
	
	int len = snprintf(status, sizeof(status), 
		" %.20s [%d] - %d:%d", 
		editor.filepath ? basename(editor.filepath) : "<new buff>", 
		editor.buf_chain->lines_num, 
		editor.cursor_y + 1,
		editor.cursor_x + 1);
	
	if (len > editor.screen_cols) len = editor.screen_cols;
	
	ab_append(ab, status, len);
	
	while (len < editor.screen_cols) 
	{
		ab_append(ab, " ", 1);
		len++;
	}
	
	ab_append(ab, "\x1b[m", 3);
	ab_append(ab, "\r\n", 2);
}

void editor_draw_message_bar(APPEND_BUFFER *ab)
{
	ab_append(ab, "\x1b[K", 3);
	
	int msglen = strlen(editor.status_msg);
	if (msglen > editor.screen_cols) msglen = editor.screen_cols;
	
	if (msglen && time(NULL) - editor.status_msg_time < 5)
		ab_append(ab, editor.status_msg, msglen);
}

void editor_refresh_screen()
{	
	APPEND_BUFFER ab = {NULL, 0};
	
	if ((editor.screen_cols < 30) 
		|| (editor.screen_rows < 10))
	{
		ab_append(&ab, "\x1b[?25l", 6);
		ab_append(&ab, "\x1b[2J", 4);
		ab_append(&ab, "\x1b[H", 3);
		ab_append(&ab, "Terminal size too small!", 24);
	}
	else
	{
		editor_scroll();
		
		ab_append(&ab, "\x1b[?25l", 6);
		ab_append(&ab, "\x1b[2J", 4);
		ab_append(&ab, "\x1b[H", 3);
		
		editor_draw_buffer(&ab);
		editor_draw_status_bar(&ab);
		editor_draw_message_bar(&ab);
		
		char buf[32];
		snprintf(buf, sizeof(buf), "\x1b[%d;%dH", 
			(editor.cursor_y - editor.row_offset) + 1, 
			(editor.cursor_rx - editor.col_offset)+ 1);
		ab_append(&ab, buf, strlen(buf));
		ab_append(&ab, "\x1b[?25h", 6);
	}
	
	write(STDOUT_FILENO, ab.b, ab.len);
	ab_free(&ab);
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
	BUFFER_NODE *current_line;
	
	current_line = buf_get_line_at(editor.buf_chain, editor.cursor_y + 1, FALSE);
	
	switch (c)
	{
		case UP:
			if (editor.cursor_y != 0)
				editor.cursor_y--;
			break;
			
		case DOWN:
			if (editor.cursor_y < editor.buf_chain->lines_num - 1)
				editor.cursor_y++;
			break;
			
		case LEFT:
			if (editor.cursor_x != 0)
			{
				editor.cursor_x--;
				editor.cache_cursor_x_snap = editor.cursor_x;
			}
			else if (editor.cursor_y > 0)
			{
				editor.cursor_y--;
				
				current_line = buf_get_line_at(editor.buf_chain, editor.cursor_y + 1, FALSE);
				editor.cursor_x = current_line->len;
				editor.cache_cursor_x_snap = editor.cursor_x;
			}
			break;	
			
		case RIGHT:
			if (current_line && editor.cursor_x < current_line->len)
			{
				editor.cursor_x++;
			}
			else if (current_line && current_line->next && editor.cursor_x == current_line->len)
			{
				editor.cursor_y++;
				editor.cursor_x = 0;
			}
			
			editor.cache_cursor_x_snap = editor.cursor_x;
			break;
	}
	
	current_line = buf_get_line_at(editor.buf_chain, editor.cursor_y + 1, FALSE);
	
	if (editor.cache_cursor_x_snap > current_line->len)
	{
		editor.cursor_x = current_line->len;
	}
	else
	{
		editor.cursor_x = editor.cache_cursor_x_snap;
	}
}

void editor_insert(int c)
{
	buf_insert(editor.buf_chain, editor.cursor_y + 1, editor.cursor_x, c);
	
	editor.cache_cursor_x_snap = ++editor.cursor_x;
}

void editor_process_keypress()
{
	int c = editor_read_key();
	
	// Refresh window dimensiones
	if (editor_get_window_size(&editor.screen_rows, &editor.screen_cols) == -1) 
		die("editor_get_window_size");
	
	if ((editor.screen_cols < 30) 
		|| (editor.screen_rows < 10)) return;
	
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
			{
				if (c == PAGE_UP) 
				{
					editor.cursor_y = editor.row_offset;
				} 
				else if (c == PAGE_DOWN) 
				{
					editor.cursor_y = editor.row_offset + editor.screen_rows - 1;
					if (editor.cursor_y > editor.buf_chain->lines_num) 
						editor.cursor_y = editor.buf_chain->lines_num;
				}
				
				int times = editor.screen_rows;
				
				while (times--)
					editor_move_cursor(c == PAGE_UP ? UP : DOWN);
			}
			break;
			
		case HOME_KEY:
			editor.cursor_x = 0;
			editor.cache_cursor_x_snap = 0;
			break;
			
		case END_KEY:
			if (editor.cursor_y < editor.buf_chain->lines_num)
			{
				BUFFER_NODE *buf_node = buf_get_line_at(editor.buf_chain, editor.cursor_y + 1, FALSE);
				
				editor.cursor_x = buf_node->len;
				editor.cache_cursor_x_snap = buf_node->len;
			}
			break;
			
		case DEL_KEY:
			// Do nothing for now
			break;
			
		default:
			editor_insert(c);
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
	editor.cursor_x = 0;
	editor.cursor_y = 0;
	editor.cursor_rx = 0;
	editor.row_offset = 0;
	editor.col_offset = 0;
	editor.buf_chain = buf_new_canvas();
	editor.filepath = NULL;
	editor.status_msg[0] = '\0';
	editor.status_msg_time = 0;
	
	if (editor_get_window_size(&editor.screen_rows, &editor.screen_cols) == -1) 
		die("editor_get_window_size");
}