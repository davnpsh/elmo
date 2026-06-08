#include <string.h>
#include <stdlib.h>

#include "abuf.h"

void ab_append_string(APPEND_BUFFER *ab, char *s)
{
	int len = strlen(s);
	char *new = realloc(ab->b, ab->len + len);
	
	if (new == NULL) return;
	
	memcpy(&new[ab->len], s, len);
	ab->b = new;
	ab->len += len;
}

void ab_append_char(APPEND_BUFFER *ab, char c)
{
    char *new = realloc(ab->b, ab->len + 1);
    
    if (new == NULL) return;
    
    new[ab->len] = c;
    ab->b = new;
    ab->len++;
}

void ab_free(APPEND_BUFFER *ab)
{
	free(ab->b);
}