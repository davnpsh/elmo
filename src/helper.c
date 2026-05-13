#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>

#include "helper.h"
#include "syntax_hl.h"

#define TAB_STOP 4

void die(const char *s) 
{
	write(STDOUT_FILENO, "\x1b[2J", 4);
	write(STDOUT_FILENO, "\x1b[H", 3);
	perror(s);
	exit(1);
}

int cx_to_rx(const char *s, int cursor_x)
{
	int cursor_rx = 0;
	
	for (int j = 0; j < cursor_x; j++)
	{
		if (s[j] == '\t')
		{
			cursor_rx += (TAB_STOP - 1) - (cursor_rx % TAB_STOP);
		}
		
		cursor_rx++;
	}
	
	return cursor_rx;
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