#include <sys/ioctl.h>
#include <unistd.h>
#include <errno.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <libgen.h>
#include <stdarg.h>
#include <ctype.h>

#include "editor.h"
#include "helper.h"
#include "syntax_hl.h"

#define CURRENT_LINE buf_get_line_at(editor.buf_chain, editor.cursor_y + 1, FALSE)
#define TRUE 1
#define FALSE 0
#define CTRL_KEY(k) ((k) & 0x1f)
#define QUIT_TIMES 2
#define RESERVED_ROWS 2

#define VERSION "1.0.0"

EDITOR editor;

void editor_set_status_msg(const char *fmt, ...) 
{
	va_list ap;
	va_start(ap, fmt);
	vsnprintf(editor.status_msg, sizeof(editor.status_msg), fmt, ap);
	va_end(ap);
	editor.status_msg_time = time(NULL);
}

int editor_set_window_size()
{
	struct winsize ws;
	
	if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &ws) == -1 
		|| ws.ws_col == 0 
		|| ws.ws_row - RESERVED_ROWS == 0) return -1;

	// Reset position on actual change
	if (editor.screen_rows != ws.ws_row - RESERVED_ROWS || editor.screen_cols != ws.ws_col)
	{
		editor.cursor_x = 0;
		editor.cursor_y = 0;

		// Skip initialization
		if (editor.screen_rows != 0 && editor.screen_cols != 0)
			editor_set_status_msg("window resized!");
	}

	editor.screen_rows = ws.ws_row - RESERVED_ROWS;	// reserve space for status and prompt bar
	editor.screen_cols = ws.ws_col;
	
	return 0;
}

void editor_open(char *filepath)
{
	editor.filepath = malloc(strlen(filepath) + 1);
	strcpy(editor.filepath, filepath);
	
	editor.buf_chain = buf_parse_file(filepath);
}

void editor_save()
{
	if (editor.filepath == NULL) 
	{
		editor_prompt("save");
		return;	// this function will get called again!
	}
	
	int err = buf_save(editor.buf_chain, editor.filepath);
	
	if (err)
		editor_set_status_msg("i/o err!: ", strerror(err));
	else
	{
		editor_set_status_msg("saved");
		editor.dirty = FALSE;
	}
}

int editor_get_editable_area_width()
{
	int padding = 0;

	padding += editor.show_line_num_gutter ? editor.line_num_gutter_width : 0;

	return editor.screen_cols - padding;
}

void editor_scroll()
{
	int current_width = editor_get_editable_area_width();
	
	render_coords(&editor.cursor_rx, &editor.cursor_ry, editor.cursor_x, editor.cursor_y, editor.buf_chain, current_width);

	if (editor.sticky_col_update)
	{
		editor.sticky_col = editor.cursor_rx;
		editor.sticky_col_update = FALSE;
	}

	// Scroll up
	if (editor.cursor_ry < editor.render_offset)
    	editor.render_offset = editor.cursor_ry;
	
	// Scroll down
	if (editor.cursor_ry >= editor.render_offset + editor.screen_rows)
    	editor.render_offset = editor.cursor_ry - editor.screen_rows + 1;
}

void editor_draw_buffer(APPEND_BUFFER *ab)
{
	int current_width = editor_get_editable_area_width();
	
	int row_offset, wrap_offset;

	get_offset_coordinates(&row_offset, &wrap_offset, editor.render_offset, editor.buf_chain, current_width);
	
	BUFFER_NODE *current_line = buf_get_line_at(editor.buf_chain, 1 + row_offset, TRUE);
	Bool reset_current_line_hl = FALSE;

	int inner_offset = wrap_offset;
	int logical_line = row_offset + 1;

	if (inner_offset > 0) logical_line++;
	
	for (int y = 0; y < editor.screen_rows; y++)
	{
		if (reset_current_line_hl)
		{
			ab_append(ab, "\x1b[0m", 4);
			reset_current_line_hl = FALSE;
		}

		if (y + editor.render_offset < get_total_display_rows(editor.buf_chain, current_width))
		{
			// Highlight current line
			if (editor.highlight_current_line && editor.cursor_ry == editor.render_offset + y)
			{
				ab_append(ab, "\x1b[48;5;236m", 11);
				reset_current_line_hl = TRUE;
			}
			
			// Line number
			if (editor.show_line_num_gutter)
			{
				char gutter_buf[32];
				int line_number;	// for display

				if (inner_offset == 0)
				{
					if (editor.line_num_position == ABSOLUTE)
						line_number = logical_line;
					else
						line_number = abs(logical_line - editor.cursor_y - 1);

					if (logical_line == editor.cursor_y + 1)
					{
						ab_append(ab, "\x1b[34m", 5);
						snprintf(gutter_buf, sizeof(gutter_buf), "%*d ", editor.line_num_gutter_width - 1, line_number);
					}
					else 
					{
						ab_append(ab, "\x1b[90m", 5);
						snprintf(gutter_buf, sizeof(gutter_buf), "%-*d ", editor.line_num_gutter_width - 1, line_number);
					}

					logical_line++;
				}
				else if (inner_offset == 1)
				{
					if (logical_line == editor.cursor_y + 2)
					{
						ab_append(ab, "\x1b[34m", 5);
						snprintf(gutter_buf, sizeof(gutter_buf), "%*s ", editor.line_num_gutter_width - 1, ">");
					}
					else
					{
						ab_append(ab, "\x1b[90m", 5);
						snprintf(gutter_buf, sizeof(gutter_buf), "%-*s ", editor.line_num_gutter_width - 1, ">");
					}
				}
				else
				{
					snprintf(gutter_buf, sizeof(gutter_buf), "%*s ", editor.line_num_gutter_width - 1, "");
				}

				ab_append(ab, gutter_buf, editor.line_num_gutter_width);
				ab_append(ab, "\x1b[39m", 5);
			}

			// Some pre-calcs

			int line_display_rows = get_line_display_rows(current_line->rlen, current_width);

			int start = (current_width) * inner_offset;

			char *r = &current_line->r[start];
			char *h = &current_line->h[start];

			int len = current_line->rlen - start;

			if (len < 0) len = 0;
			if (len > current_width)
				len = current_width;

			if (inner_offset == line_display_rows - 1)
			{
				current_line = current_line->next;
				inner_offset = 0;
			}
			else
				inner_offset++;

			// Print line

			int current_color = -1;	// default

			for (int j = 0; j < len; j++)
			{
				// Adjust color
				if (h[j] == TK_NORMAL) 
				{
					if (current_color != -1)
					{
						ab_append(ab, "\x1b[39m", 5);
						current_color = -1;
					}
				}
				else
				{
					int color = token_to_color(h[j]);

					if (color != current_color)
					{
						current_color = color;
						char buf[16];
						int clen = snprintf(buf, sizeof(buf), "\x1b[%dm", color);
						ab_append(ab, buf, clen);
					}
				}

				// Print char
				ab_append(ab, &r[j], 1);
			}
			
			ab_append(ab, "\x1b[39m", 5);
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
		" %.20s%s [%s] - %s - %d/%d:%d", 
		editor.filepath ? basename(editor.filepath) : "<new buff>", 
		editor.dirty ? "~" : "",
		editor.buf_chain->syntax ? 
			((SYNTAX *)editor.buf_chain->syntax)->filetype : "/",
		(editor.mode == SAFE) ? "s" : "e",
		editor.cursor_y + 1,
		editor.buf_chain->lines_num,
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

void editor_draw_welcome(APPEND_BUFFER *ab)
{
	// Draw Dorothy!!!!!!!!!!!
	const char *dorothy[] = {
		"  )............(  ",
		" /              \\",
		"|                |",
		"|          <><   |",
		" \\              / ",
		"  ')__________('  "
	};
	int dorothy_size = sizeof(dorothy) / sizeof(dorothy[0]);

	int x_padding, y_padding;

	y_padding = (editor.screen_rows - dorothy_size) / 2;

	while (y_padding--)
	{
		ab_append(ab, "\x1b[K", 3);
		ab_append(ab, "\r\n", 2);
	}

	for (int i = 0; i < 6; i++)
	{
		x_padding = (editor.screen_cols - strlen(dorothy[i])) / 2;

		while (x_padding--) ab_append(ab, " ", 1);

		ab_append(ab, dorothy[i], strlen(dorothy[i]));

		ab_append(ab, "\x1b[K", 3);
		ab_append(ab, "\r\n", 2);
	}

	ab_append(ab, "\x1b[K", 3);
	ab_append(ab, "\r\n", 2);

	// Welcome message
	const char *welcome = "welcome to elmo!";
	
	x_padding = (editor.screen_cols - strlen(welcome)) / 2;

	while (x_padding--) ab_append(ab, " ", 1);

	ab_append(ab, welcome, strlen(welcome));

	ab_append(ab, "\x1b[K", 3);
	ab_append(ab, "\r\n\n", 3);

	// Version
	char version_str[80];
	int version_len = snprintf(version_str, sizeof(version_str), "version %s", VERSION);

	x_padding = (editor.screen_cols - version_len) / 2;

	while (x_padding--) ab_append(ab, " ", 1);

	ab_append(ab, version_str, version_len);

	ab_append(ab, "\x1b[K", 3);
	ab_append(ab, "\r\n", 2);

	// Author
	const char *author = "by daru";
	
	x_padding = (editor.screen_cols - strlen(author)) / 2;

	while (x_padding--) ab_append(ab, " ", 1);

	ab_append(ab, author, strlen(author));
}

void editor_refresh_screen(Bool in_prompt)
{	
	APPEND_BUFFER ab = {NULL, 0};
	
	if (editor.buf_chain == NULL)
	{
		ab_append(&ab, "\x1b[?25l", 6);
		ab_append(&ab, "\x1b[2J", 4);
		ab_append(&ab, "\x1b[H", 3);

		editor_draw_welcome(&ab);
	}
	else
	{
		editor.line_num_gutter_width = 0;
		if (editor.show_line_num_gutter)
		{
			editor.line_num_gutter_width = digit_count(editor.buf_chain->lines_num) 
				// Padding left or right
				+ 1
				// Space separator
				+ 1;
		}
		
		editor_scroll();
		
		ab_append(&ab, "\x1b[?25l", 6);
		ab_append(&ab, "\x1b[2J", 4);
		ab_append(&ab, "\x1b[H", 3);
		
		editor_draw_buffer(&ab);
		editor_draw_status_bar(&ab);
		editor_draw_message_bar(&ab);
		
		char buf[32];
		int x, y;
		
		if (in_prompt)
		{
			x = editor.cursor_px + 1;
			y = editor.screen_rows + 2;
		}
		else
		{
			x = editor.cursor_rx + 1 + editor.line_num_gutter_width;
			y = editor.cursor_ry - editor.render_offset + 1;
		}
		
		snprintf(buf, sizeof(buf), "\x1b[%d;%dH", y, x);
			
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
	 	char seq[5];
			
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
				else if (seq[2] == ';')
				{
					if (read(STDIN_FILENO, &seq[3], 1) != 1) return '\x1b';
            		if (read(STDIN_FILENO, &seq[4], 1) != 1) return '\x1b';

              		if (seq[3] == '5')
                	{
                 		switch (seq[4])
                   		{
                     		case 'A': return CTRL_UP;
		                    case 'B': return CTRL_DOWN;
                            case 'C': return CTRL_RIGHT;
                            case 'D': return CTRL_LEFT;
                     	}
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
	
	current_line = CURRENT_LINE;
	
	switch (c)
	{
		case UP:
		{
			if (editor.cursor_ry == 0) break;
			
			int editable_area_width = editor_get_editable_area_width();
			
			int current_row_offset, current_wrap_offset;	
			get_offset_coordinates(&current_row_offset, &current_wrap_offset, editor.cursor_ry, editor.buf_chain, editable_area_width);

			// Moving up, but in the same logical line
			if (current_wrap_offset > 0)
			{
				// editor.cursor_x -= editable_area_width;
				editor.cursor_x = editable_area_width * (current_wrap_offset - 1) + editor.sticky_col;
			}
			// Moving up, changing y
			else if (editor.cursor_y != 0)
			{
				int prev_row_offset, prev_wrap_offset;
				get_offset_coordinates(&prev_row_offset, &prev_wrap_offset, editor.cursor_ry - 1, editor.buf_chain, editable_area_width);

				editor.cursor_y--;

				current_line = CURRENT_LINE;

				if (
					((current_line->len - editable_area_width * prev_wrap_offset) < editor.cursor_x)
					|| ((current_line->len - editable_area_width * prev_wrap_offset) < editor.sticky_col)
				)
				{
					editor.cursor_x = current_line->len;
				}
				else
				{
					// editor.cursor_x += editable_area_width * prev_wrap_offset;
					// editor.cursor_x = editor.sticky_col;
					editor.cursor_x = editable_area_width * prev_wrap_offset + editor.sticky_col;
				}
			}
		}
			break;
			
		case DOWN:
		{
			int editable_area_width = editor_get_editable_area_width();
			
			if (editor.cursor_ry > get_total_display_rows(editor.buf_chain, editable_area_width)) break;

			int current_row_offset, current_wrap_offset;	
			get_offset_coordinates(&current_row_offset, &current_wrap_offset, editor.cursor_ry, editor.buf_chain, editable_area_width);

			int next_row_offset, next_wrap_offset;
			get_offset_coordinates(&next_row_offset, &next_wrap_offset, editor.cursor_ry + 1, editor.buf_chain, editable_area_width);

			// Moving down, but in the same logical line
			if (current_row_offset == next_row_offset)
			{
				current_line = CURRENT_LINE;
				
				if ((current_line->len - editable_area_width * next_wrap_offset) < (editor.cursor_x - editable_area_width * current_wrap_offset))
				{
					editor.cursor_x = current_line->len;
				}
				else
				{
					editor.cursor_x += editable_area_width;
				}
			}
			// Moving down, changing y
			else if (editor.cursor_y < editor.buf_chain->lines_num - 1)
			{
				editor.cursor_y++;
				
				current_line = CURRENT_LINE;
				
				if (
					(current_line->len < (editor.cursor_x - editable_area_width * current_wrap_offset))
					|| (current_line->len < editor.sticky_col)
				)
				{
					editor.cursor_x = current_line->len;
				}
				else
				{
					editor.cursor_x = editor.sticky_col;
				}
			}
		}
			break;
			
		case LEFT:
			if (editor.cursor_x != 0)
			{
				editor.cursor_x--;
			}
			else if (editor.cursor_y > 0)
			{
				editor.cursor_y--;
				
				current_line = CURRENT_LINE;
				editor.cursor_x = current_line->len;
			}

			editor.sticky_col_update = TRUE;
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

			editor.sticky_col_update = TRUE;
			break;
	}
}

void editor_move_word(int c)
{
	BUFFER_NODE *current_line = CURRENT_LINE;
	int pos = editor.cursor_x;
	
	if (c == CTRL_LEFT)
	{
		if (editor.cursor_x == 0)
		{
			editor_move_cursor(LEFT);
			return;
		}
		
		while (pos > 0 && current_line->s[pos - 1] == ' ') pos--;
		while (pos > 0 && current_line->s[pos - 1] != ' ') pos--;
	}
	else
	{
		if (editor.cursor_x == current_line->len)
		{
			editor_move_cursor(RIGHT);
			return;
		}

		while (pos < current_line->len && current_line->s[pos + 1] == ' ') pos++;
		while (pos < current_line->len && current_line->s[pos + 1] != ' ') pos++;
	}

	editor.cursor_x = pos;
	editor.sticky_col_update = TRUE;
}

void editor_insert(int c)
{
	buf_insert(editor.buf_chain, editor.cursor_y + 1, editor.cursor_x, c);
	
	if (c == '\r')
	{
		editor.cursor_x = 0;
		editor.cursor_y++;
	}
	else editor.cursor_x++;
	
	editor.dirty = TRUE;
}

void editor_delete()
{
	if (editor.cursor_x == 0 && editor.cursor_y == 0) return;
	
	int len = 0;
	
	if (editor.cursor_y > 0)
	{
		BUFFER_NODE *prev_line = buf_get_line_at(editor.buf_chain, editor.cursor_y, FALSE);
		len = prev_line->len;
	}
	
	buf_delete(editor.buf_chain, editor.cursor_y + 1, editor.cursor_x);
	
	if (editor.cursor_x > 0)
	{
		editor.cursor_x--;
	}
	else if (editor.cursor_y > 0)
	{	
		editor.cursor_y--;
		editor.cursor_x = len;
	}
	
	editor.dirty = TRUE;
}

void editor_process_command(char* command)
{
	// Parsing the command
	command++; //	delete '/'
	
	char *pch;
	pch = strtok(command, " ");

	// Save
	if (strcmp(pch, "save") == 0)
	{
		pch = strtok(NULL, " ");
		
		if (pch != NULL)
		{
			free(editor.filepath);
			editor.filepath = malloc(strlen(pch) + 1);
			strcpy(editor.filepath, pch);
			editor_save();
		}
		else
		{
			editor_save();
		}
	}
	// Toggle line number gutter
	else if (strcmp(pch, "linenumbers") == 0
	 	  || strcmp(pch, "nums") == 0)
	{
		pch = strtok(NULL, " ");

		if (pch == NULL)
		{
			editor.show_line_num_gutter = !editor.show_line_num_gutter;
		}
		else if (strcmp(pch, "rel") == 0)
		{
			editor.line_num_position = RELATIVE;
		}
		else if (strcmp(pch, "abs") == 0)
		{
			editor.line_num_position = ABSOLUTE;
		}
		else
		{
			editor_set_status_msg("what?");
		}
	}
	else if (strcmp(pch, "syntax") == 0)
	{
		pch = strtok(NULL, " ");
		
		if (pch != NULL)
		{
			SYNTAX *syntax = get_syntax_by_filetype(pch);

			if (syntax == NULL) 
			{
				editor_set_status_msg("invalid filetype!");
				return;
			}

			editor.buf_chain->syntax = syntax;
			buf_update_syntax_hl(editor.buf_chain);
		}
		else
		{
			editor_set_status_msg("what?");
		}
	}
	else if (strcmp(pch, "quit") == 0)
	{
		if (editor.dirty)
		{
			editor_set_status_msg("use /quit! to exit without saving.");
		}
		else
		{
			write(STDOUT_FILENO, "\x1b[2J", 4);
			write(STDOUT_FILENO, "\x1b[H", 3);
			exit(0);
		}
	}
	else if (strcmp(pch, "quit!") == 0)
	{
		write(STDOUT_FILENO, "\x1b[2J", 4);
		write(STDOUT_FILENO, "\x1b[H", 3);
		exit(0);
	}
	else
	{
		editor_set_status_msg("invalid command!");
	}
}

void editor_prompt(const char *command)
{
	size_t buf_size = 128;
	char *buf = malloc(buf_size);
	
	size_t buf_len = 1;
	buf[0] = '/';
	buf[1] = '\0';
	
	editor.cursor_px = 1;
	
	if (command != NULL)
	{
		int len = (int)strlen(command);
		
		memcpy(&buf[1], command, len);
		buf[len + 1] = '\0';
		
		editor.cursor_px += len;
		buf_len += len;
	}
	
	while (1)
	{
		editor_set_status_msg(buf);
		editor_refresh_screen(TRUE);
		
		int c = editor_read_key();
		
		switch(c)
		{
			case HOME_KEY:
				editor.cursor_px = 1;
				break;
				
			case END_KEY:
				editor.cursor_px = (int)buf_len;
				break;
				
			case DEL_KEY:
			case BACKSPACE:
				if (c == DEL_KEY)
				{
					if (editor.cursor_px == (int)buf_len) break;
					editor.cursor_px++;
				}
				
				if (buf_len - 1 == 0)
				{
					editor_set_status_msg("");
					free(buf);
					return;
				}
				
				if (editor.cursor_px > 0)
				{
					if (editor.cursor_px != 1 || buf_len <= 1)
					{
						memmove(&buf[editor.cursor_px - 1], 
							&buf[editor.cursor_px], 
							buf_len - editor.cursor_px);
						
						buf[--buf_len] = '\0';
						
						editor.cursor_px--;
					}
				}
				break;
				
			case LEFT:
				if (editor.cursor_px > 1)
					editor.cursor_px--;
				break;
				
			case RIGHT:
				if (editor.cursor_px < (int)buf_len)
					editor.cursor_px++;
				break;
				
			case '\x1b':
				editor_set_status_msg("");
				free(buf);
				return;
				
			case '\r':
				editor_set_status_msg("");
				editor_process_command(buf);
				free(buf);
				return;
			
			default:
				if (!iscntrl(c) && c < 128) 
				{
					if (buf_len == buf_size - 1)
					{
						buf_size *= 2;
						buf = realloc(buf, buf_size);
					}
					
					buf[buf_len++] = c;
					buf[buf_len] = '\0';
					
					editor.cursor_px++;
				}
		}
	}
}

void editor_process_keypress()
{
	static int quit_times = QUIT_TIMES;
	
	int c = editor_read_key();
	
	// Refresh window dimensiones
	if (editor_set_window_size() == -1) 
		die("editor_set_window_size");

	// On any keypress, just start a new chain
	if (editor.buf_chain == NULL) 
	{
		editor.buf_chain = buf_new_chain();
	}
	
	switch (c)
	{
		case CTRL_KEY('q'):
			if (editor.dirty && quit_times > 0)
			{
				editor_set_status_msg("unsaved buffer! press ^q %d more time(s) to quit", quit_times);
				quit_times--;
				return;
			}
			
			write(STDOUT_FILENO, "\x1b[2J", 4);
			write(STDOUT_FILENO, "\x1b[H", 3);
			exit(0);
			break;
			
		case CTRL_KEY('s'):
      		editor_save();
        	break;
         
        case CTRL_KEY('e'):
         	editor.mode = (editor.mode == SAFE) ? EDIT : SAFE;
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
				int times = editor.screen_rows - 1;
				
				while (times--)
				{
					editor_move_cursor(c == PAGE_UP ? UP : DOWN);
					editor_scroll();
				}
			}
			break;
			
		case HOME_KEY:
			editor.cursor_x = 0;
			editor.sticky_col_update = TRUE;
			break;
			
		case END_KEY:
			if (editor.cursor_y < editor.buf_chain->lines_num)
			{
				BUFFER_NODE *buf_node = CURRENT_LINE;
				
				editor.cursor_x = buf_node->len;
				editor.sticky_col_update = TRUE;
			}
			break;
			
		case DEL_KEY:
		case BACKSPACE:
			if (editor.mode == SAFE) break;
			
			if (c == DEL_KEY)
			{
				BUFFER_NODE *buf_node = CURRENT_LINE;
				
				// Avoid DEL on last x,y position
				if (editor.cursor_x == buf_node->len 
					&& editor.cursor_y == editor.buf_chain->lines_num - 1) break;
				
				editor_move_cursor(RIGHT);
			}
			
			editor_delete();
			break;
			
		case CTRL_KEY('l'):
		case CTRL_KEY('h'):
		case '\x1b':
		    break;

		// case CTRL_UP:
		// case CTRL_DOWN:
		case CTRL_LEFT:
		case CTRL_RIGHT:
			editor_move_word(c);
			break;
						
		case '/':
			if (editor.mode == SAFE)
			{
				editor_prompt(NULL);
				break;
			}
			/* fall-through */
		case '\r':
		default:
			if (editor.mode == SAFE) break;
			
			editor_insert(c);
			break;
	}
	
	quit_times = QUIT_TIMES;
}

void init_editor() 
{
	editor.cursor_x = 0;
	editor.cursor_y = 0;
	editor.cursor_rx = 0;
	editor.cursor_ry = 0;
	editor.render_offset = 0;
	editor.sticky_col = 0;
	editor.sticky_col_update = FALSE;
	editor.dirty = FALSE;
	editor.highlight_current_line = TRUE;
	editor.show_line_num_gutter = TRUE;
	editor.line_num_position = ABSOLUTE;
	editor.line_num_gutter_width = 0;
	editor.mode = SAFE;
	editor.buf_chain = NULL;
	editor.filepath = NULL;
	editor.status_msg[0] = '\0';
	editor.status_msg_time = 0;
	
	if (editor_set_window_size() == -1) 
		die("editor_set_window_size");
}