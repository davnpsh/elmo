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
	const char *extension = strrchr(filepath, '.');

	for (unsigned int i = 0; i < SYNTAX_ENTRIES; i++)
	{
		SYNTAX *syntax = &syntax_db[i];

		for (int j = 0; syntax->filematch[j] != NULL; j++)
		{
			if (strcmp(extension, syntax->filematch[j]) == 0)
				return syntax;
		}
	}
	
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