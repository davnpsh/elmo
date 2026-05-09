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
	else if (*c == '"' || *c == '\'')
	{
		char quote = *c;
		
		// skip opening quote
		c++;
		(*len)++;

		while (*c && *c != quote)
		{
			c++;
			(*len)++;
		}

		(*len)++;

		if (*c == quote) return TK_STRING;
		else return TK_NORMAL;
	}
	// else if (isalpha(*c) || *c == '_')
	// {
	// 	char buf[256];
		
	// }
	// Default
	else
	{
		*len = 1;
		return TK_NORMAL;
	}
}