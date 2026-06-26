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

#define MAX(a, b) ((a) > (b) ? (a) : (b))
#define CURRENT_LINE buf_get_line_at(editor.buf_chain, editor.cursor.y + 1, FALSE)

EDITOR editor;

void editor_cleanup()
{
	buf_free_chain(editor.buf_chain);
    free(editor.filepath);
    free(editor.clipboard);
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
	render_coords(editor.buf_chain, editor.cursor, &editor.cursor_render);

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

void editor_draw_line_number(APPEND_BUFFER *ab, int line_num, int wrap_offset)
{
	if (!editor.show_line_num_gutter) return;
	
	char gutter_buf[32];

	if (wrap_offset == 0)
	{
		Bool is_cursor_line = (line_num == editor.cursor.y + 1) && !editor.in_prompt;
		int display_num = (editor.line_num_mode == ABSOLUTE || is_cursor_line)
			            ? line_num
			            : abs(line_num - editor.cursor.y - 1);

		if (is_cursor_line)
		{
			ab_append_esc_seq(ab, ESC_FG_BLUE);
			snprintf(gutter_buf, sizeof(gutter_buf), "%*d ", 
				editor.line_num_gutter_width - 1, display_num);
		}
		else 
		{
			ab_append_esc_seq(ab, ESC_FG_GRAY);
			snprintf(gutter_buf, sizeof(gutter_buf), "%-*d ", 
				editor.line_num_gutter_width - 1, display_num);
		}
	}
	else if (wrap_offset == 1)
	{
		Bool is_cursor_wrap = (line_num - 1 == editor.cursor.y + 1) && !editor.in_prompt;
		
		if (is_cursor_wrap)
		{
			ab_append_esc_seq(ab, ESC_FG_BLUE);
			snprintf(gutter_buf, sizeof(gutter_buf), "%*s ", 
				editor.line_num_gutter_width - 1, ">");
		}
		else
		{
			ab_append_esc_seq(ab, ESC_FG_GRAY);
			snprintf(gutter_buf, sizeof(gutter_buf), "%-*s ", 
				editor.line_num_gutter_width - 1, ">");
		}
	}
	else
	{
		snprintf(gutter_buf, sizeof(gutter_buf), "%*s ", 
			editor.line_num_gutter_width - 1, "");
	}

	ab_append_string(ab, gutter_buf);
	ab_append_esc_seq(ab, ESC_RESET_FG);
}

void editor_draw_buffer(APPEND_BUFFER *ab)
{
	int current_width = editor_get_editable_area_width();
	
	int row_offset, wrap_offset;
	get_offset_coordinates(editor.buf_chain, 
						editor.render_offset, 
						&row_offset, 
						&wrap_offset);
	
	BUFFER_NODE *current_line = buf_get_line_at(editor.buf_chain, 1 + row_offset, TRUE);
	int line_num = row_offset + 1;
	Bool line_hl_active = FALSE;
	
	if (wrap_offset > 0) line_num++;
	
	for (int y = 0; y < editor.screen_rows; y++)
	{
		int display_row = editor.render_offset + y;
		Bool in_selection = is_row_in_selection(editor.text_selected, 
												editor.r_select_start, 
												editor.r_select_end, 
												display_row);
		
		// Reset user line highlight
  		if (line_hl_active)
        {
            ab_append_esc_seq(ab, ESC_RESET_BG);
            line_hl_active = FALSE;
        }

    	// -- WELCOME SCREEN --
    	// On fresh start, welcome screen takes over after the first row
		if (y == 1 
			&& editor.welcome
			&& editor.screen_rows >= MIN_ROWS_FOR_WELCOME
			&& editor.screen_cols >= MIN_COLS_FOR_WELCOME) break;

		if (display_row >= editor.buf_chain->total_display_rows)
        {
            ab_append_esc_seq(ab, ESC_CLEAR_LINE);
            ab_append_esc_seq(ab, ESC_CARRIAGE_RETURN);
            continue;
        }

		// -- HIGHLIGHT CURRENT LINE --
		if (!editor.text_selected 
			&& !editor.in_prompt
			&& editor.highlight_current_line
			&& editor.cursor_render.y == display_row)
		{
			ab_append_esc_seq(ab, ESC_BG_HL);
			line_hl_active = TRUE;
		}
		
		// -- LINE NUMBER --
		editor_draw_line_number(ab, line_num, wrap_offset);
		if (wrap_offset == 0) line_num++;

		// -- PRE-COMPUTATION OF VISIBLE SEGMENT OF LINE --
		int render_start = current_width * wrap_offset;
        char *render_ptr = &current_line->r[render_start];
        char *hl_ptr = &current_line->h[render_start];
        int visible_len = current_line->rlen - render_start;
 
        if (visible_len < 0) visible_len = 0;
        if (visible_len > current_width) visible_len = current_width;
 
        if (wrap_offset == current_line->display_wrap_rows - 1)
        {
            current_line = current_line->next;
            wrap_offset = 0;
        }
        else
            wrap_offset++;
		
		int current_color = -1;

		if (in_selection)
            ab_append_esc_seq(ab, ESC_BG_SELECT);

		// -- EMPTY LINE MARKER --
		// (Visual cue of selection of empty lines)
		if (visible_len == 0)
		{
   			SELECTION_STATE sel = get_selection_state(editor.text_selected, 
      												editor.r_select_start, 
                    								editor.r_select_end, 
                              						display_row, 
                                      				0, 
                                          			in_selection,
                                            		current_width);
 
            if (sel == SEL_START || sel == SEL_INSIDE)
            {
                ab_append_esc_seq(ab, ESC_BG_SELECT);
                ab_append_string(ab, " ");
                ab_append_esc_seq(ab, ESC_RESET_BG);
                in_selection = (sel != SEL_END);
            }
            else if (sel == SEL_END)
            {
                ab_append_string(ab, " ");
                ab_append_esc_seq(ab, ESC_RESET_BG);
                in_selection = FALSE;
            }
            else if (in_selection)
            {
                ab_append_string(ab, " ");
                ab_append_esc_seq(ab, ESC_RESET_BG);
            }
		}
		else
		{
   			for (int j = 0; j < visible_len; j++)
            {
                SELECTION_STATE sel = get_selection_state(editor.text_selected, 
                										editor.r_select_start, 
                              							editor.r_select_end, 
                                        				display_row, 
                                            			j, 
                                                		in_selection,
                                                    	current_width);
                
                switch (sel)
                {
                    case SEL_START:
                        ab_append_esc_seq(ab, ESC_BG_SELECT);
                        in_selection = TRUE;
                        break;
                        
                    case SEL_END:
                        ab_append_esc_seq(ab, ESC_RESET_BG);
                        in_selection = FALSE;
                        break;
                        
                    case SEL_INSIDE:
                    case SEL_NONE:
                        break;
                }

                // Syntax color
                if (hl_ptr[j] == TK_NORMAL)
                {
                    if (current_color != -1)
                    {
                        ab_append_esc_seq(ab, ESC_RESET_FG);
                        current_color = -1;
                    }
                }
                else
                {
                    int color = token_to_color(hl_ptr[j]);
                    if (color != current_color)
                    {
                        current_color = color;
                        char buf[16];
                        snprintf(buf, sizeof(buf), "\x1b[%dm", color);
                        ab_append_esc_seq(ab, buf);
                    }
                }

                // Print character
                ab_append_char(ab, render_ptr[j]);
            }
 
            if (in_selection)
                ab_append_esc_seq(ab, ESC_RESET_BG);
		}
		
		ab_append_esc_seq(ab, ESC_RESET_FG);
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
			get_offset_coordinates(editor.buf_chain, editor.cursor_render.y, &current_row_offset, &current_wrap_offset);

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
				get_offset_coordinates(editor.buf_chain, editor.cursor_render.y - 1, &prev_row_offset, &prev_wrap_offset);

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
			get_offset_coordinates(editor.buf_chain, editor.cursor_render.y, &current_row_offset, &current_wrap_offset);

			int next_row_offset, next_wrap_offset;
			get_offset_coordinates(editor.buf_chain, editor.cursor_render.y + 1, &next_row_offset, &next_wrap_offset);

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
		editor.select_anchor = (POSITION){ editor.cursor.x, editor.cursor.y };
	}
	
	switch (c)
	{
		case SHIFT_UP: editor_move_cursor(UP); break;
		case SHIFT_DOWN: editor_move_cursor(DOWN); break;
		case SHIFT_LEFT: editor_move_cursor(LEFT); break;
		case SHIFT_RIGHT: editor_move_cursor(RIGHT); break;
	}

	if ((editor.select_anchor.x == editor.cursor.x) 
		&& (editor.select_anchor.y == editor.cursor.y)) 
	{
		editor.text_selected = FALSE;
		return;
	}

	Bool is_cursor_after = (editor.cursor.y > editor.select_anchor.y)
        || (editor.cursor.y == editor.select_anchor.y
            && editor.cursor.x > editor.select_anchor.x);

	if (is_cursor_after)
	{
		editor.select_start = editor.select_anchor;
        editor.select_end = (POSITION){ editor.cursor.x, editor.cursor.y };
	}
	else
	{
		editor.select_start = (POSITION){ editor.cursor.x, editor.cursor.y };
        editor.select_end = editor.select_anchor;
	}
	
	render_coords(editor.buf_chain, editor.select_start, &editor.r_select_start);
	render_coords(editor.buf_chain, editor.select_end, &editor.r_select_end);
}

void editor_insert(int c)
{
	if (editor.text_selected)
	{
		buf_insert_block(editor.buf_chain, editor.select_start, editor.select_end, c);

		editor.cursor = editor.select_start;
		editor.text_selected = FALSE;

		if (c == '\r')
		{
			editor.cursor.x = 0;
			editor.cursor.y++;
		}
		else editor.cursor.x++;
	}
	else
	{
		buf_insert(editor.buf_chain, editor.cursor.y + 1, editor.cursor.x, c);

		// Auto-pairs
		if (c == '{' || c == '(' || c == '[')
		{
			editor.cursor.x++;
			
			int p = (c == '{') ? '}' :
					(c == '(') ? ')' : ']';
			buf_insert(editor.buf_chain, editor.cursor.y + 1, editor.cursor.x, p);
		}
		// New-line
		else if (c == '\r')
		{
			// Auto-identation
			BUFFER_NODE *line;

			line = CURRENT_LINE;
			
			int offset = 0;
			while (offset < line->rlen && line->r[offset] == ' ')
				offset++;
			
			editor.cursor.y++;
			
			line = CURRENT_LINE;
			
			int index = 0;
			
			for (int i = 0; i < offset / TAB_STOP; i++)
				buf_insert(editor.buf_chain, editor.cursor.y + 1, index++, '\t');

			for (int i = 0; i < offset % TAB_STOP; i++)
				buf_insert(editor.buf_chain, editor.cursor.y + 1, index++, ' ');
			
			editor.cursor.x = rx_to_cx(line->s, line->len, offset);
		}
		else editor.cursor.x++;
	}

	editor.dirty = TRUE;
}

void editor_delete()
{
	if (editor.text_selected)
	{
		buf_delete_block(editor.buf_chain, editor.select_start, editor.select_end);
		
		editor.cursor = editor.select_start;
		editor.text_selected = FALSE;
	}
	else
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
	}

	editor.sticky_col_update = TRUE;
	editor.dirty = TRUE;
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

void editor_toggle_comment()
{
	// Comment block
	if (editor.text_selected)
	{
		int shift_first_line, shift_last_line;

		int result = buf_toggle_comment_block(editor.buf_chain, 
			editor.select_start.y + 1, 
			editor.select_end.y + 1, 
			&shift_first_line, 
			&shift_last_line);

		if (result == -1) return;

		int direction = (result == 1) ? -1 : 1;

		if (editor.cursor.y == editor.select_start.y)
            editor.cursor.x = MAX(0, editor.cursor.x + direction * shift_first_line);
        else
            editor.cursor.x = MAX(0, editor.cursor.x + direction * shift_last_line);

		editor.select_start.x = MAX(0, editor.select_start.x + direction * shift_first_line);
        editor.select_end.x = MAX(0, editor.select_end.x   + direction * shift_last_line);

        if (editor.select_anchor.y == editor.select_start.y)
            editor.select_anchor.x = editor.select_start.x;
        else
            editor.select_anchor.x = editor.select_end.x;

		render_coords(editor.buf_chain, editor.select_start, &editor.r_select_start);
		render_coords(editor.buf_chain, editor.select_end, &editor.r_select_end);
	}
	// Comment line
	else
	{
		int shift;

		int result = buf_toggle_comment_line(editor.buf_chain, editor.cursor.y + 1, &shift);

		if (result == -1) return;

		int direction = (result == 1) ? -1 : 1;
		
        editor.cursor.x = MAX(0, editor.cursor.x + direction * shift);
	}

	editor.sticky_col_update = TRUE;
   	editor.dirty = TRUE;
}

void editor_copy()
{
	// Copy block
	if (editor.text_selected)
	{
		free(editor.clipboard);
		editor.clipboard = buf_read_selection(editor.buf_chain, editor.select_start, editor.select_end);
		editor.line_copy = FALSE;
		editor_set_status_msg("text copied!");
	}
	// Copy entire line
	else
	{
		free(editor.clipboard);
		editor.clipboard = buf_read_line(editor.buf_chain, editor.cursor.y + 1);
		editor.line_copy = TRUE;
		editor_set_status_msg("text copied!");
	}
}

void editor_paste()
{
	if (editor.clipboard == NULL) return;

	int cursor_x = editor.cursor.x;

	if (editor.line_copy)
	{
		editor.cursor.x = 0;
		editor_insert('\r');
		editor.cursor.y--;
	}
		
	for (size_t i = 0; i < strlen(editor.clipboard); i++)
		editor_insert(editor.clipboard[i]);

	if (editor.line_copy)
	{
		editor.cursor.y++;
		editor.cursor.x = cursor_x;
	}

	editor.sticky_col_update = TRUE;
	editor.dirty = TRUE;
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
	int c = editor_read_key();

	// On any keypress, disable welcome message
	if (editor.welcome) 
		editor.welcome = FALSE;

	// Enforce safe mode
	if ((!iscntrl(c) && c < 128 && c != '/')
		|| c == 31 // CTRL+/
		|| c == DEL_KEY
		|| c == BACKSPACE
		|| c == CTRL_KEY('v'))
		if (editor.mode == SAFE) return;
	
	switch (c)
	{	
		case CTRL_KEY('s'):
      		editor_save();
        	break;
         
        case CTRL_KEY('e'):
         	editor.mode = (editor.mode == SAFE) ? EDIT : SAFE;
           	break;

        case /* CTRL+/ */ 31:
			editor_toggle_comment();
			break;

		case CTRL_KEY('c'):
			editor_copy();
			break;

		case CTRL_KEY('v'):
			editor_paste();
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

			BUFFER_NODE *buf_node = CURRENT_LINE;

			int offset = 0;
			while (offset < buf_node->len && (buf_node->s[offset] == '\t' || buf_node->s[offset] == ' '))
				offset++;

			if (offset > 0 && editor.cursor.x == offset)
				editor.cursor.x = 0;
			else
				editor.cursor.x = offset;
			
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
			if ((!iscntrl(c) && c < 128) || c == '\r' || c == '\t')
				editor_insert(c);
			break;
	}
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
	editor.line_copy = FALSE;
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
