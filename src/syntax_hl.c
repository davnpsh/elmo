#include <string.h>
#include <stdlib.h>
#include <ctype.h>

#include "syntax_hl.h"
#include "bufchn.h"

char *C_extensions[] = { ".c", ".h", ".cpp", NULL };
char *C_keywords[] = {
    /* C89/C90 */
    "auto",     "break",    "case",     "char",
    "const",    "continue", "default",  "do",
    "double",   "else",     "enum",     "extern",
    "float",    "for",      "goto",     "if",
    "int",      "long",     "register", "return",
    "short",    "signed",   "sizeof",   "static",
    "struct",   "switch",   "typedef",  "union",
    "unsigned", "void",     "volatile", "while",
    /* C99 */
    "_Bool",    "_Complex", "_Imaginary", "inline",
    "restrict",
    NULL
};

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
	if ((isdigit(*c) || (*c == '-' && isdigit(*(c + 1))))
	 && (syntax->flags & HL_HIGHLIGHT_NUMBERS))
	{
		c++;
		(*len)++;
		
		while (isdigit(*c) || *c == '.')
		{
			c++;
			(*len)++;
		}

		return TK_NUMBER;
	}
	// Strings
	else if ((*c == '"' || *c == '\'') 
		&& (syntax->flags & HL_HIGHLIGHT_STRINGS))
	{
		char quote = *c;
		
		// Skip opening quote
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
	// Identifiers and keywords
	else if (isalpha(*c) || *c == '_')
	{
		char buf[256];
		
		while (isalnum(*c) || *c == '_') buf[(*len)++] = *(c++);
		
		buf[*len] = '\0';

		for (int i = 0; syntax->keywords[i] != NULL; i++)
		{
			if (strcmp(buf, syntax->keywords[i]) == 0) return TK_KEYWORD;
		}

		return TK_IDENTIFIER;
	}
	// Single-line comments
	else if (syntax->singleline_comment != NULL
		&& (strncmp(c, syntax->singleline_comment, strlen(syntax->singleline_comment)) == 0))
	{
		while (*c)
		{
			c++;
			(*len)++;
		}

		return TK_COMMENT;
	}
	// Language specifics
	else
	{
		// C
		if (strcmp(syntax->filetype, "c") == 0)
		{
			// System includes
			if (*c == '<')
			{
				c++;
				(*len)++;
				
				while (isalnum(*c) || *c == '.')
				{
					c++;
					(*len)++;
				}

				(*len)++;

				if (*c == '>') return TK_STRING;
			}
		}
		
		// Default
		*len = 1;
		return TK_NORMAL;
	}
}

void syntax_hl_update(SYNTAX *syntax, void *buf_node)
{
	BUFFER_NODE *node = (BUFFER_NODE *)buf_node;
	
	node->h = realloc(node->h, node->rlen);
	
	int len, idx = 0;

	while (idx < node->rlen)
	{
		char token = tokenize(syntax, &(node->r)[idx], &len);

		memset(&(node->h)[idx], token, len);

		idx += len;
	}
}