#include <string.h>
#include <stdlib.h>

#include "abuf.h"

void abAppend(ABUF *ab, const char *s, int len)
{
	char *new = realloc(ab->b, ab->len + len);
	
	if (new == NULL) return;
	
	memcpy(&new[ab->len], s, len);
	ab->b = new;
	ab->len += len;
}

void abFree(ABUF *ab)
{
	free(ab->b);
}