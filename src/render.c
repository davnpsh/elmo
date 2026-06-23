#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <ctype.h>

#include "render.h"
#include "syntax.h"
#include "bufchn.h"
#include "editor.h"

int rx_to_cx(char *s, int len, int cursor_rx)
{
    int rx = 0;
    
    for (int i = 0; i < len; i++)
    {
        if (s[i] == '\t')
            rx += TAB_STOP - (rx % TAB_STOP);
        else
            rx++;
        
        if (rx > cursor_rx) return i;
    }
    
    return len;
}

int cx_to_rx(char *s, int cursor_x)
{
	int cursor_rx = 0;
	
	for (int j = 0; j < cursor_x; j++)
	{
		if (s[j] == '\t')
			cursor_rx += (TAB_STOP - 1) - (cursor_rx % TAB_STOP);
		
		cursor_rx++;
	}
	
	return cursor_rx;
}

int get_line_wrap_rows(int line_len, int width)
{
	if (line_len == 0) return 1;
	return (line_len + width - 1) / width;
}

void update_layout(BUFFER_CHAIN *buf_chain, int width)
{
	if (buf_chain->editor_width_cache == width 
		&& buf_chain->total_display_rows != 0
		&& !buf_chain->update_layout)
        return;

	BUFFER_NODE *current = buf_chain->head;
    int total = 0;

    while (current)
    {
    	current->display_row_offset = total;
    	current->display_wrap_rows = get_line_wrap_rows(current->rlen, width);
        total += current->display_wrap_rows;
        current = current->next;
    }

    buf_chain->total_display_rows = total;
    buf_chain->editor_width_cache = width;
}

void render_coords(BUFFER_CHAIN *buf_chain, POSITION cursor, POSITION *cursor_render)
{
	BUFFER_NODE *line = buf_get_line_at(buf_chain, cursor.y + 1, FALSE);

	int rx_pos = cx_to_rx(line->s, cursor.x);
	cursor_render->y = line->display_row_offset + rx_pos / buf_chain->editor_width_cache;
	cursor_render->x = rx_pos % buf_chain->editor_width_cache;
}

void get_offset_coordinates(BUFFER_CHAIN *buf_chain, int ry, int *row_offset, int *wrap_offset)
{
	int r_offset = 0;
	int w_offset = 0;

	BUFFER_NODE *current_line = buf_chain->head;

	while (ry > 0)
	{
		int lines = current_line->display_wrap_rows;

		if (lines > ry) 
		{
			w_offset = ry;
			break;
		}
		else
		{
			ry -= lines;
			r_offset++;
		}
		
		current_line = current_line->next;
	}

	*row_offset = r_offset;
	*wrap_offset = w_offset;
}

Bool is_row_in_selection(Bool text_selected, POSITION render_select_start, POSITION render_select_end, int display_row)
{
	if (!text_selected) return FALSE;

	// This is for totally selected rows (those IN THE MIDDLE)
	// AND the row in which the selected text ends.
	// (tl;dr: this excludes the first selected row)
	return display_row > render_select_start.y
        && display_row <= render_select_end.y;
}

SELECTION_STATE get_selection_state(Bool text_selected, POSITION render_select_start, POSITION render_select_end, int display_row, int ry, Bool in_selection, int width)
{
	if (!text_selected) return SEL_NONE;

	Bool on_start_row = (display_row == render_select_start.y);
	Bool on_end_row = (display_row == render_select_end.y);
	int start_col = render_select_start.x % width;
	int end_col = render_select_end.x % width;

	if (on_start_row && ry == start_col) return SEL_START;
	if (on_end_row && ry == end_col) return SEL_END;

	return in_selection ? SEL_INSIDE : SEL_NONE;
}

int token_to_color(unsigned char token)
{
	switch(token)
	{
		case TK_NUMBER: return 31;
		case TK_STRING: return 32;
		case TK_COMMENT: return 90;
		case TK_KEYWORD: return 33;
		default: return 37;
	}
}

CHAR_TYPE get_char_type(char c)
{
	if (isalpha(c) || c == '_') return WORD;
	if (c == ' ' || c == '\t') return WHITESPACE;
	
	return PUNCTUATION;
}

int digit_count(int n)
{
	char buf[32];
    sprintf(buf, "%d", abs(n));
    return strlen(buf);
}