#include <ctype.h>

#include "token.h"

TOKEN tokenize(char *s, int *len)
{
	char *c;

	c = s;
	*len = 0;

	// Number
	if (isdigit(*c))
	{
		while (isdigit(*c) || *c == '.')
		{
			c++;
			(*len)++;
		}

		return TK_NUMBER;
	}
	// Default
	else
	{
		*len = 1;
		return TK_NORMAL;
	}
}