#include <string.h>
#include <stdlib.h>
#include <ctype.h>

#include "syntax_hl.h"

char *C_extensions[] = { ".c", ".h", ".cpp", NULL };
char *C_keywords[] = { "if", NULL };

SYNTAX syntax_db[] = {
	{
		"c",
		C_extensions,
		C_keywords,
		"//",
		"/*",
		"*/",
		HL_HIGHLIGHT_NUMBERS | HL_HIGHLIGHT_STRINGS
	}
};

SYNTAX *get_syntax(const char *filepath)
{
	return NULL;
}

char tokenize(SYNTAX *syntax, char *s, int *len)
{
	if (syntax == NULL)
	{
		*len = 1;
		return TK_NORMAL;
	}
	
	char *c;

	c = s;
	*len = 0;

	// Number
	if (isdigit(*c) && (syntax->flags & HL_HIGHLIGHT_NUMBERS))
	{
		while (isdigit(*c) || *c == '.')
		{
			c++;
			(*len)++;
		}

		return TK_NUMBER;
	}
	else if ((*c == '"' || *c == '\'') && (syntax->flags & HL_HIGHLIGHT_STRINGS))
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

void syntax_hl_update(SYNTAX *syntax, char **h, char **r, int rlen)
{
	*h = realloc(*h, rlen);
	
	int len, idx = 0;

	while (idx < rlen)
	{
		char token = tokenize(syntax, &(*r)[idx], &len);

		memset(&(*h)[idx], token, len);

		idx += len;
	}
}