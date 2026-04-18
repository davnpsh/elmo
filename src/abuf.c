#include <string.h>
#include <stdlib.h>

#include "abuf.h"

void ab_append(APPEND_BUFFER *ab, const char *s, int len)
{
	char *new = realloc(ab->b, ab->len + len);
	
	if (new == NULL) return;
	
	memcpy(&new[ab->len], s, len);
	ab->b = new;
	ab->len += len;
}

void ab_free(APPEND_BUFFER *ab)
{
	free(ab->b);
}