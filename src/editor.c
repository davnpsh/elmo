#define _DEFAULT_SOURCE

#include <sys/ioctl.h>
#include <unistd.h>
#include <errno.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <libgen.h>
#include <stdarg.h>
#include <ctype.h>
#include <signal.h>

#include "editor.h"
#include "abuf.h"
#include "syntax.h"
#include "bufchn.h"
#include "render.h"
#include "command.h"
#include "util.h"

#define CURRENT_LINE buf_get_line_at(editor.buf_chain, editor.cursor.y + 1, FALSE)

EDITOR editor;

void editor_cleanup()
{
	buf_free_chain(editor.buf_chain);
    free(editor.filepath);
}

void editor_set_status_msg(char *fmt, ...) 
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
		editor.cursor.x = 0;
		editor.sticky_col = 0;
		editor.cursor.y = 0;
	}

	editor.screen_rows = ws.ws_row - RESERVED_ROWS;	// reserve space for status and prompt bar
	editor.screen_cols = ws.ws_col;
	
	return 0;
}

void editor_handle_window_resize(int sig)
{
	(void)sig;
	
	if (editor_set_window_size() == -1) 
		die("editor_set_window_size");

	editor_refresh_screen();
}

void editor_open(char *filepath)
{
	editor.filepath = malloc(strlen(filepath) + 1);
	strcpy(editor.filepath, filepath);

	buf_free_chain(editor.buf_chain);
	
	editor.buf_chain = buf_parse_file(filepath);
	editor.welcome = FALSE;
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
	render_coords(&editor.cursor_render, editor.cursor, editor.buf_chain);

	if (editor.sticky_col_update)
	{
		editor.sticky_col = editor.cursor_render.x % editor_get_editable_area_width();
		editor.sticky_col_update = FALSE;
	}

	// Scroll up
	if (editor.cursor_render.y < editor.render_offset)
    	editor.render_offset = editor.cursor_render.y;
	
	// Scroll down
	int below_visible_rows = 2;
	
	if ((editor.cursor_render.y >= editor.render_offset + editor.screen_rows - below_visible_rows)
		&& (editor.render_offset + editor.screen_rows < editor.buf_chain->total_display_rows))
	{
		editor.render_offset = editor.cursor_render.y - editor.screen_rows + 1 + below_visible_rows;
	}
}

void editor_draw_line_number(APPEND_BUFFER *ab, int *number, int offset)
{
	if (!editor.show_line_num_gutter) return;
	
	char gutter_buf[32];
	int line_number;	// for display

	if (offset == 0)
	{
		if (editor.line_num_mode == ABSOLUTE || (*number == editor.cursor.y + 1))
			line_number = *number;
		else
			line_number = abs(*number - editor.cursor.y - 1);

		if ((*number == editor.cursor.y + 1) && !editor.in_prompt)
		{
			ab_append_esc_seq(ab, ESC_FG_BLUE);
			snprintf(gutter_buf, sizeof(gutter_buf), "%*d ", editor.line_num_gutter_width - 1, line_number);
		}
		else 
		{
			ab_append_esc_seq(ab, ESC_FG_GRAY);
			snprintf(gutter_buf, sizeof(gutter_buf), "%-*d ", editor.line_num_gutter_width - 1, line_number);
		}

		(*number)++;
	}
	else if (offset == 1)
	{
		if ((*number - 1 == editor.cursor.y + 1) && !editor.in_prompt)
		{
			ab_append_esc_seq(ab, ESC_FG_BLUE);
			snprintf(gutter_buf, sizeof(gutter_buf), "%*s ", editor.line_num_gutter_width - 1, ">");
		}
		else
		{
			ab_append_esc_seq(ab, ESC_FG_GRAY);
			snprintf(gutter_buf, sizeof(gutter_buf), "%-*s ", editor.line_num_gutter_width - 1, ">");
		}
	}
	else
	{
		snprintf(gutter_buf, sizeof(gutter_buf), "%*s ", editor.line_num_gutter_width - 1, "");
	}

	ab_append_string(ab, gutter_buf);
	ab_append_esc_seq(ab, ESC_RESET_FG);
}

void editor_draw_buffer(APPEND_BUFFER *ab)
{
	int current_width = editor_get_editable_area_width();
	
	int row_offset, wrap_offset;

	get_offset_coordinates(&row_offset, &wrap_offset, editor.render_offset, editor.buf_chain);
	
	BUFFER_NODE *current_line = buf_get_line_at(editor.buf_chain, 1 + row_offset, TRUE);

	int inner_offset = wrap_offset;
	int displayed_line_number = row_offset + 1;
	
	Bool reset_current_line_hl = FALSE;
	Bool highlighting_selected_text = FALSE;

	if (inner_offset > 0) displayed_line_number++;

	if (editor.text_selected && editor.render_offset > editor.r_select_start.y)
		highlighting_selected_text = TRUE;
	
	for (int y = 0; y < editor.screen_rows; y++)
	{
		if (reset_current_line_hl)
		{
			ab_append_esc_seq(ab, ESC_RESET_BG);
			reset_current_line_hl = FALSE;
		}

		if (y == 1 && editor.welcome
			&& editor.screen_rows >= MIN_ROWS_FOR_WELCOME
			&& editor.screen_cols >= MIN_COLS_FOR_WELCOME) break;

		if (y + editor.render_offset < editor.buf_chain->total_display_rows)
		{
			// -- HIGHLIGHT CURRENT LINE --
			if (!editor.text_selected 
				&& !editor.in_prompt
				&& editor.highlight_current_line 
				&& editor.cursor_render.y == editor.render_offset + y)
			{
				ab_append_esc_seq(ab, ESC_BG_HL);
				reset_current_line_hl = TRUE;
			}
			
			// -- LINE NUMBER --
			editor_draw_line_number(ab, &displayed_line_number, inner_offset);

			// -- PRE-CALCS --

			// Calculate which segment of the logical part
			// print in the display line
			int start = current_width * inner_offset;

			char *r = &current_line->r[start];
			char *h = &current_line->h[start];

			int len = current_line->rlen - start;

			if (len < 0) len = 0;
			if (len > current_width) len = current_width;

			if (inner_offset == current_line->display_wrap_rows - 1)
			{
				current_line = current_line->next;
				inner_offset = 0;
			}
			else
				inner_offset++;

			// -- PRINT LINE --
			if (highlighting_selected_text)
			{
				ab_append_esc_seq(ab, ESC_BG_SELECT);
				
				if (len == 0)
				{
					ab_append_string(ab, " ");
					ab_append_esc_seq(ab, ESC_RESET);
				}
			}

			if (editor.text_selected && len == 0)
			{
				// Start of the selected text
				if (editor.render_offset + y == editor.r_select_start.y)
				{
					ab_append_esc_seq(ab, ESC_BG_SELECT);
					ab_append_string(ab, " ");
					ab_append_esc_seq(ab, ESC_RESET);
					highlighting_selected_text = TRUE;
				}

				// End of the selected text
				if (editor.render_offset + y == editor.r_select_end.y)
				{
					ab_append_string(ab, " ");
					ab_append_esc_seq(ab, ESC_RESET);
					highlighting_selected_text = FALSE;
				}
			}

			int current_color = -1;	// default

			for (int j = 0; j < len; j++)
			{
				// Highlight selected text
				if (editor.text_selected)
				{
					// Start of the selected text
					if (editor.render_offset + y == editor.r_select_start.y)
					{
						if (j == editor.r_select_start.x % current_width)
						{
							ab_append_esc_seq(ab, ESC_BG_SELECT);
							highlighting_selected_text = TRUE;
						}
					}

					// End of the selected text
					if (editor.render_offset + y == editor.r_select_end.y)
					{
						if (j == editor.r_select_end.x % current_width)
						{
							ab_append_esc_seq(ab, ESC_RESET);
							highlighting_selected_text = FALSE;
						}
					}
				}
				
				// Adjust color for text
				if (h[j] == TK_NORMAL) 
				{
					if (current_color != -1)
					{
						ab_append_esc_seq(ab, ESC_RESET_FG);
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
						snprintf(buf, sizeof(buf), "\x1b[%dm", color);
						ab_append_esc_seq(ab, buf);
					}
				}

				// Print character
				ab_append_char(ab, r[j]);

				if (highlighting_selected_text && j == len - 1)
					ab_append_esc_seq(ab, ESC_RESET);
			}
			
			ab_append_esc_seq(ab, ESC_RESET_FG);
		}
		
		ab_append_esc_seq(ab, ESC_CLEAR_LINE);
		ab_append_esc_seq(ab, ESC_CARRIAGE_RETURN);
	}
}

void editor_draw_status_bar(APPEND_BUFFER *ab)
{
	ab_append_esc_seq(ab, ESC_REVERSE);
	
	char status[100];
	
	int len = snprintf(status, sizeof(status), 
		" %.20s%s [%s] - %s - %d/%d:%d", 
		editor.filepath ? basename(editor.filepath) : "<new buff>", 
		editor.dirty ? "~" : "",
		editor.buf_chain->syntax ? 
			((SYNTAX *)editor.buf_chain->syntax)->filetype : "/",
		(editor.mode == SAFE) ? "s" : "e",
		editor.cursor.y + 1,
		editor.buf_chain->lines_num,
		editor.cursor.x + 1);
	
	if (len > editor.screen_cols) len = editor.screen_cols;
	
	ab_append_string(ab, status);
	
	while (len < editor.screen_cols) 
	{
		ab_append_string(ab, " ");
		len++;
	}
	
	ab_append_esc_seq(ab, ESC_RESET);
	ab_append_esc_seq(ab, ESC_CARRIAGE_RETURN);
}

void editor_draw_message_bar(APPEND_BUFFER *ab)
{
	ab_append_esc_seq(ab, ESC_CLEAR_LINE);
	
	int msglen = strlen(editor.status_msg);
	if (msglen > editor.screen_cols) msglen = editor.screen_cols;
	
	if (msglen && time(NULL) - editor.status_msg_time < 5)
		ab_append_string(ab, editor.status_msg);
}

void editor_draw_welcome(APPEND_BUFFER *ab)
{
	char *dorothy[] = {
        "  )............(  ",
        "/        o     \\",
        "|       O        |",
        "|        o       |",
        " \\         <><  / ",
        "  ')__________('  "
    };
    int dorothy_size = sizeof(dorothy) / sizeof(dorothy[0]);

    char version_str[80];
    snprintf(version_str, sizeof(version_str), "version %s", VERSION);

    char *lines[16];
    int line_count = 0;

    lines[line_count++] = NULL;            // writing line

    for (int i = 0; i < dorothy_size; i++)
        lines[line_count++] = dorothy[i];

    lines[line_count++] = NULL;            // blank line
    lines[line_count++] = "welcome to elmo!";
    lines[line_count++] = version_str;

    lines[line_count++] = "by daru";

    int y_padding = (editor.screen_rows - line_count) / 2;

    while (y_padding--)
    {
        ab_append_esc_seq(ab, ESC_CLEAR_LINE);
        ab_append_esc_seq(ab, ESC_CARRIAGE_RETURN);
    }

    for (int i = 0; i < line_count; i++)
    {
        char *text = lines[i] ? lines[i] : "";
        int x_padding = (editor.screen_cols - (int)strlen(text)) / 2;

        while (x_padding-- > 0) ab_append_string(ab, " ");

        ab_append_string(ab, text);

        ab_append_esc_seq(ab, ESC_CLEAR_LINE);
        ab_append_esc_seq(ab, ESC_CARRIAGE_RETURN);
    }

    y_padding = editor.screen_rows - ((editor.screen_rows - line_count) / 2) - line_count - 1;

    while (y_padding--)
    {
        ab_append_esc_seq(ab, ESC_CLEAR_LINE);
        ab_append_esc_seq(ab, ESC_CARRIAGE_RETURN);
    }
}

void editor_refresh_screen()
{
	APPEND_BUFFER ab = AB_INIT;
		
	update_layout(editor.buf_chain, editor_get_editable_area_width());
	
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
	
	ab_append_esc_seq(&ab, ESC_HIDE_CURSOR);
	ab_append_esc_seq(&ab, ESC_CLEAR_SCREEN);
	ab_append_esc_seq(&ab, ESC_CURSOR_HOME);
	
	editor_draw_buffer(&ab);

	if (editor.welcome 
		&& editor.screen_rows >= MIN_ROWS_FOR_WELCOME 
		&& editor.screen_cols >= MIN_COLS_FOR_WELCOME)
		editor_draw_welcome(&ab);
	
	editor_draw_status_bar(&ab);
	editor_draw_message_bar(&ab);
	
	char buf[32];
	int x, y;
	
	if (editor.in_prompt)
	{
		x = editor.cursor_px + 1;
		y = editor.screen_rows + 2;
	}
	else
	{
		x = editor.cursor_render.x + 1 + editor.line_num_gutter_width;
		y = editor.cursor_render.y - editor.render_offset + 1;
	}
	
	snprintf(buf, sizeof(buf), "\x1b[%d;%dH", y, x);
		
	ab_append_esc_seq(&ab, buf);
	ab_append_esc_seq(&ab, ESC_SHOW_CURSOR);
	
	write(STDOUT_FILENO, ab.b, ab.len);
	ab_free(&ab);
}

int editor_read_key()
{
	int nread;
	char c;
	
	while ((nread = read(STDIN_FILENO, &c, 1)) != 1)
	{
		if (nread == -1 
			&& errno != EAGAIN 
			&& errno != EINTR) 
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
	                else if (seq[3] == '2')
	                {
	                    switch (seq[4])
	                    {
	                        case 'A': return SHIFT_UP;
	                        case 'B': return SHIFT_DOWN;
	                        case 'C': return SHIFT_RIGHT;
	                        case 'D': return SHIFT_LEFT;
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
			if (editor.cursor_render.y == 0) break;
			
			int editable_area_width = editor_get_editable_area_width();
			
			int current_row_offset, current_wrap_offset;	
			get_offset_coordinates(&current_row_offset, &current_wrap_offset, editor.cursor_render.y, editor.buf_chain);

			// Moving up, but in the same logical line
			if (current_wrap_offset > 0)
			{
				int rx = editable_area_width * (current_wrap_offset - 1) + editor.sticky_col;
				editor.cursor.x = rx_to_cx(current_line->s, current_line->len, rx);
			}
			// Moving up, changing y
			else if (editor.cursor.y != 0)
			{
				int prev_row_offset, prev_wrap_offset;
				get_offset_coordinates(&prev_row_offset, &prev_wrap_offset, editor.cursor_render.y - 1, editor.buf_chain);

				editor.cursor.y--;

				current_line = CURRENT_LINE;

				int rx = editable_area_width * prev_wrap_offset + editor.sticky_col;
				editor.cursor.x = rx_to_cx(current_line->s, current_line->len, rx);
			}
		}
			break;
			
		case DOWN:
		{
			int editable_area_width = editor_get_editable_area_width();

			update_layout(editor.buf_chain, editable_area_width);
			
			if (editor.cursor_render.y > editor.buf_chain->total_display_rows) break;

			int current_row_offset, current_wrap_offset;	
			get_offset_coordinates(&current_row_offset, &current_wrap_offset, editor.cursor_render.y, editor.buf_chain);

			int next_row_offset, next_wrap_offset;
			get_offset_coordinates(&next_row_offset, &next_wrap_offset, editor.cursor_render.y + 1, editor.buf_chain);

			// Moving down, but in the same logical line
			if (current_row_offset == next_row_offset)
			{
				int rx = editable_area_width * next_wrap_offset + editor.sticky_col;
				editor.cursor.x = rx_to_cx(current_line->s, current_line->len, rx);
			}
			// Moving down, changing y
			else if (editor.cursor.y < editor.buf_chain->lines_num - 1)
			{
				editor.cursor.y++;
				
				current_line = CURRENT_LINE;

				int rx = editor.sticky_col;
				editor.cursor.x = rx_to_cx(current_line->s, current_line->len, rx);
			}
		}
			break;
			
		case LEFT:
			if (editor.cursor.x != 0)
			{
				editor.cursor.x--;
			}
			else if (editor.cursor.y > 0)
			{
				editor.cursor.y--;
				
				current_line = CURRENT_LINE;
				editor.cursor.x = current_line->len;
			}

			editor.sticky_col_update = TRUE;
			break;	
			
		case RIGHT:
			if (current_line && editor.cursor.x < current_line->len)
			{
				editor.cursor.x++;
			}
			else if (current_line && current_line->next && editor.cursor.x == current_line->len)
			{
				editor.cursor.y++;
				editor.cursor.x = 0;
			}

			editor.sticky_col_update = TRUE;
			break;
	}
}

void editor_move_by_word(int c)
{
	BUFFER_NODE *current_line = CURRENT_LINE;
	int pos = editor.cursor.x;
	
	if (c == CTRL_LEFT)
	{
		if (editor.cursor.x == 0)
		{
			editor_move_cursor(LEFT);
			return;
		}
		
		CHAR_TYPE initial_ctype = get_char_type(current_line->s[pos - 1]);
		while (pos > 0
			&& initial_ctype == get_char_type(current_line->s[pos - 1])) pos--;
	}
	else
	{
		if (editor.cursor.x == current_line->len)
		{
			editor_move_cursor(RIGHT);
			return;
		}

		CHAR_TYPE initial_ctype = get_char_type(current_line->s[pos]);
		while (pos < current_line->len 
			&& initial_ctype == get_char_type(current_line->s[pos])) pos++;
	}

	editor.cursor.x = pos;
	editor.sticky_col_update = TRUE;
}

void editor_select(int c)
{
	if (editor.text_selected == FALSE)
	{
		editor.text_selected = TRUE;
		editor.select_start = (POSITION){ editor.cursor.x, editor.cursor.y };
	}
	
	switch (c)
	{
		case SHIFT_UP:
			editor_move_cursor(UP);
			break;
			
		case SHIFT_DOWN:
			editor_move_cursor(DOWN);
			break;

		case SHIFT_LEFT:
			editor_move_cursor(LEFT);
			break;
			
		case SHIFT_RIGHT:
			editor_move_cursor(RIGHT);
			break;
	}

	if ((editor.select_start.x == editor.cursor.x) 
		&& (editor.select_start.y == editor.cursor.y)) 
	{
		editor.text_selected = FALSE;
		return;
	}

	editor.select_end = (POSITION){ editor.cursor.x, editor.cursor.y };

	POSITION start, end;

	if (
		(editor.select_start.y < editor.cursor.y)
		|| ((editor.select_start.y == editor.cursor.y) 
			&& (editor.select_start.x < editor.cursor.x))
	)
	{
		start = editor.select_start;
		end = editor.select_end;
	}
	else
	{
		start = editor.select_end;
		end = editor.select_start;
	}
	
	render_coords(&editor.r_select_start, start, editor.buf_chain);
	render_coords(&editor.r_select_end, end, editor.buf_chain);
}

void editor_insert(int c)
{
	buf_insert(editor.buf_chain, editor.cursor.y + 1, editor.cursor.x, c);
	
	if (c == '\r')
	{
		editor.cursor.x = 0;
		editor.cursor.y++;
	}
	else editor.cursor.x++;
	
	editor.dirty = TRUE;
	editor.buf_chain->update_layout = TRUE;
}

void editor_jump(int shift)
{
	int target;
	
	if (editor.line_num_mode == ABSOLUTE)
	{
		target = shift - 1;
		
		if (!(target >= 0 && target < editor.buf_chain->lines_num))
		{
			editor_set_status_msg("invalid line!");
			return;
		}
	}
	else
	{
		target = editor.cursor.y + shift;

		if (!(target >= 0 && target < editor.buf_chain->lines_num))
		{
			editor_set_status_msg("invalid line!");
			return;
		}
	}

	editor.cursor.x = 0;
	editor.cursor.y = target;
}

void editor_delete()
{
	if (editor.cursor.x == 0 && editor.cursor.y == 0) return;
	
	int len = 0;
	
	if (editor.cursor.y > 0)
	{
		BUFFER_NODE *prev_line = buf_get_line_at(editor.buf_chain, editor.cursor.y, FALSE);
		len = prev_line->len;
	}
	
	buf_delete(editor.buf_chain, editor.cursor.y + 1, editor.cursor.x);
	
	if (editor.cursor.x > 0)
	{
		editor.cursor.x--;
	}
	else if (editor.cursor.y > 0)
	{	
		editor.cursor.y--;
		editor.cursor.x = len;
	}
	
	editor.dirty = TRUE;
	editor.buf_chain->update_layout = TRUE;
}

void editor_prompt(char *command)
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

	editor.in_prompt = TRUE;
	
	while (1)
	{
		editor_set_status_msg(buf);
		editor_refresh_screen();
		
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
				
				if (buf_len == 1)
				{
					editor.in_prompt = FALSE;
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
				editor.in_prompt = FALSE;
				editor_set_status_msg("");
				free(buf);
				return;
				
			case '\r':
				if (buf_len != 1)
				{
					editor.in_prompt = FALSE;
					editor_set_status_msg("");
					process_command(buf);
					free(buf);
					return;
				}
				break;
			
			default:
				if (!iscntrl(c) && c < 128) 
				{
					if (buf_len == buf_size - 1)
					{
						buf_size *= 2;
						buf = realloc(buf, buf_size);
					}
					
					memmove(&buf[editor.cursor_px + 1],
						&buf[editor.cursor_px],
						buf_len - editor.cursor_px + 1);

					buf_len++;

					buf[editor.cursor_px] = c;
					
					editor.cursor_px++;
				}
		}
	}
}

void editor_process_keypress()
{
	// static int quit_times = QUIT_TIMES;
	
	int c = editor_read_key();

	// On any keypress, disable welcome message
	if (editor.welcome) 
		editor.welcome = FALSE;
	
	switch (c)
	{
		// case CTRL_KEY('q'):
		// 	if (editor.dirty && quit_times > 0)
		// 	{
		// 		editor_set_status_msg("unsaved buffer! press ^q %d more time(s) to quit", quit_times);
		// 		quit_times--;
		// 		return;
		// 	}
			
		// 	write(STDOUT_FILENO, "\x1b[2J", 4);
		// 	write(STDOUT_FILENO, "\x1b[H", 3);
		// 	exit(0);
		// 	break;
			
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
			editor.text_selected = FALSE;
			editor_move_cursor(c);
			break;
			
		case PAGE_UP:
		case PAGE_DOWN:
			editor.text_selected = FALSE;
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
			editor.text_selected = FALSE;
			editor.cursor.x = 0;
			editor.sticky_col_update = TRUE;
			break;
			
		case END_KEY:
			editor.text_selected = FALSE;
			if (editor.cursor.y < editor.buf_chain->lines_num)
			{
				BUFFER_NODE *buf_node = CURRENT_LINE;
				
				editor.cursor.x = buf_node->len;
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
				if (editor.cursor.x == buf_node->len 
					&& editor.cursor.y == editor.buf_chain->lines_num - 1) break;
				
				editor_move_cursor(RIGHT);
			}
			
			editor_delete();
			break;
			
		case CTRL_KEY('l'):
		case CTRL_KEY('h'):
			break; 
			
		case '\x1b':
			if (editor.mode == EDIT) editor.mode = SAFE;
		    break;

		case CTRL_LEFT:
		case CTRL_RIGHT:
			editor_move_by_word(c);
			break;

		case SHIFT_UP:
		case SHIFT_DOWN:
		case SHIFT_LEFT:
		case SHIFT_RIGHT:
			editor_select(c);
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
			if (editor.mode == SAFE)
				editor_set_status_msg("you are on safe mode!");
			else
				editor_insert(c);
			break;
	}
	
	// quit_times = QUIT_TIMES;
}

void init_editor() 
{
	editor.cursor = (POSITION){0, 0};
	editor.cursor_render = (POSITION){0, 0};
	editor.in_prompt = FALSE;
	editor.cursor_px = 0;
	editor.render_offset = 0;
	editor.sticky_col = 0;
	editor.sticky_col_update = FALSE;
	editor.text_selected = FALSE;
	editor.dirty = FALSE;
	editor.welcome = TRUE;
	editor.highlight_current_line = TRUE;
	editor.show_line_num_gutter = TRUE;
	editor.line_num_mode = ABSOLUTE;
	editor.line_num_gutter_width = 0;
	editor.mode = SAFE;
	editor.buf_chain = buf_new_chain();
	editor.filepath = NULL;
	editor.status_msg[0] = '\0';
	editor.status_msg_time = 0;

	// Window resize
	struct sigaction winch_act;
	winch_act.sa_handler = editor_handle_window_resize;
	sigaction(SIGWINCH, &winch_act, NULL);
	
	if (editor_set_window_size() == -1) 
		die("editor_set_window_size");
}
