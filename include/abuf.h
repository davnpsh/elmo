#ifndef APPEND_BUFFER_H
#define APPEND_BUFFER_H

#define AB_INIT {NULL, 0}

typedef struct APPEND_BUFFER
{
    char *b;
    int len;
} APPEND_BUFFER;

#define ESC_RESET           	"\x1b[0m"
#define ESC_RESET_FG        	"\x1b[39m"
#define ESC_RESET_BG       		"\x1b[49m"
#define ESC_HIDE_CURSOR     	"\x1b[?25l"
#define ESC_SHOW_CURSOR     	"\x1b[?25h"
#define ESC_CLEAR_LINE      	"\x1b[K"
#define ESC_CARRIAGE_RETURN     "\r\n"
#define ESC_CLEAR_SCREEN    	"\x1b[2J"
#define ESC_CURSOR_HOME     	"\x1b[H"
#define ESC_FG_GRAY         	"\x1b[90m"
#define ESC_FG_BLUE         	"\x1b[34m"
#define ESC_REVERSE         	"\x1b[7m"
#define ESC_BG_HL           	"\x1b[48;5;235m"
#define ESC_BG_SELECT       	"\x1b[48;5;238m"

#define ab_append_esc_seq(ab, s) ab_append_string(ab, s)

void ab_append_string(APPEND_BUFFER *ab, char *s);
void ab_append_char(APPEND_BUFFER *ab, char c);
void ab_free(APPEND_BUFFER *ab);

#endif