#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>

#include "render.h"
#include "syntax.h"
#include "bufchn.h"

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

int get_line_display_rows(int line_len, int width)
{
	if (line_len == 0) return 1;
	return (line_len + width - 1) / width;
}

int get_total_display_rows(BUFFER_CHAIN *buf_chain, int width)
{
	BUFFER_NODE *current = buf_chain->head;
	int total = 0;

	while (current)
	{
		total += get_line_display_rows(current->rlen, width);
		current = current->next;
	}

	return total;
}

void render_coords(int *rx, int *ry, int x, int y, BUFFER_CHAIN *buf_chain, int width)
{
	BUFFER_NODE *current_line = buf_chain->head;
	int display_row = 0;

	for (int i = 0; i < y && current_line; i++)
	{
		display_row += get_line_display_rows(current_line->rlen, width);
		current_line = current_line->next;
	}

	int rx_pos = cx_to_rx(current_line->s, x);

	*ry = display_row + rx_pos / width;
	*rx = rx_pos % width;
}

void get_offset_coordinates(int *row_offset, int *wrap_offset, int ry, BUFFER_CHAIN *buf_chain, int width)
{
	int r_offset = 0;
	int w_offset = 0;

	BUFFER_NODE *current_line = buf_chain->head;

	while (ry > 0)
	{
		int lines = get_line_display_rows(current_line->rlen, width);

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

int digit_count(int n)
{
	char buf[32];
    sprintf(buf, "%d", abs(n));
    return strlen(buf);
}