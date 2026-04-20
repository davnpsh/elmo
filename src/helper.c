#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <libgen.h>

#include "helper.h"

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