#include <string.h>
#include <stdlib.h>
#include <ctype.h>
#include <libgen.h>

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

char *Makefile_names[] = { "Makefile", "makefile", "GNUmakefile", NULL };
char *Makefile_keywords[] = {
    /* directives */
    "define",    "endef",      "undefine",
    "ifdef",     "ifndef",     "ifeq",       "ifneq",
    "else",      "endif",
    "include",   "-include",   "sinclude",
    "override",  "export",     "unexport",
    "vpath",     "private",

    /* built-in functions */
    "subst",     "patsubst",   "strip",
    "findstring","filter",     "filter-out",
    "sort",      "word",       "words",      "wordlist",
    "firstword", "lastword",
    "dir",       "notdir",     "suffix",     "basename",
    "addsuffix", "addprefix",  "join",
    "wildcard",  "realpath",   "abspath",
    "error",     "warning",    "info",
    "shell",     "origin",     "flavor",
    "foreach",   "if",         "or",         "and",
    "call",      "eval",       "file",       "value",
    "let",       "intcmp",

    NULL
};

char *Python_extensions[] = { ".py", ".pyw", ".pyi", NULL };
char *Python_keywords[] = {
    /* control flow */
    "if",       "elif",     "else",     "for",
    "while",    "break",    "continue", "pass",
    "return",   "yield",
    /* exceptions */
    "try",      "except",   "finally",  "raise",
    "with",     "as",
    /* definitions */
    "def",      "class",    "lambda",   "async",
    "await",
    /* imports */
    "import",   "from",
    /* scope */
    "global",   "nonlocal", "del",
    /* operators */
    "and",      "or",       "not",      "in",
    "not in",   "is",       "is not",
    /* literals */
    "True",     "False",    "None",
    /* other */
    "assert",   "match",    "case",     "type",
    NULL
};

char *Markdown_extensions[] = { ".md", NULL };
char *Markdown_keywords[] = { NULL };

SYNTAX syntax_db[] = {
	{
		"c",
		C_extensions,
		C_keywords,
		"//",
		"/*",
		"*/",
		HL_HIGHLIGHT_NUMBERS | HL_HIGHLIGHT_STRINGS
	},
	{
		"makefile",
		Makefile_names,
		Makefile_keywords,
		"#",
		NULL,
		NULL,
		0
	},
	{
		"python",
		Python_extensions,
		Python_keywords,
		"#",
		NULL,
		NULL,
		HL_HIGHLIGHT_NUMBERS | HL_HIGHLIGHT_STRINGS
	},
	{
		"markdown",
		Markdown_extensions,
		Markdown_keywords,
		NULL,
		NULL,
		NULL,
		0
	}
};

SYNTAX *get_syntax_by_filetype(char *filetype)
{
	for (unsigned int i = 0; i < SYNTAX_ENTRIES; i++)
	{
		SYNTAX *syntax = &syntax_db[i];

		if (strcmp(syntax->filetype, filetype) == 0)
			return syntax;
	}

	return NULL;
}

SYNTAX *get_syntax_by_filematch(char *filepath)
{
	const char *extension = strrchr(filepath, '.');
	
	// Match by extension
	if (extension != NULL)
	{
		for (unsigned int i = 0; i < SYNTAX_ENTRIES; i++)
		{
			SYNTAX *syntax = &syntax_db[i];
			
			for (int j = 0; syntax->filematch[j] != NULL; j++)
			{
				if (strcmp(extension, syntax->filematch[j]) == 0)
					return syntax;
			}
		}
	}
	// Match by filename
	else
	{
		for (unsigned int i = 0; i < SYNTAX_ENTRIES; i++)
		{
			SYNTAX *syntax = &syntax_db[i];
			
			for (int j = 0; syntax->filematch[j] != NULL; j++)
			{
				if (strcmp(filepath, syntax->filematch[j]) == 0)
					return syntax;
			}
		}
	}

	return NULL;
}

int tokenize(SYNTAX *syntax, char *s, int *len, char **multiline_end, char **last_token)
{
	if (syntax == NULL)
	{
		*len = 1;
		return TK_NORMAL;
	}
	
	char *c;

	c = s;
	*len = 0;

	// For multi-line comments and strings terminators
	if (*multiline_end != NULL)
	{
		while (*c)
		{
			if (strncmp(c, *multiline_end, strlen(*multiline_end)) == 0)
			{
				(*len) += strlen(*multiline_end);
				*multiline_end = NULL;
				return **last_token;
			}

			c++;
			(*len)++;
		}

		return **last_token;
	}

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
	// Multi-line comments
	else if (syntax->multiline_comment_start != NULL 
		&& (strncmp(c, syntax->multiline_comment_start, strlen(syntax->multiline_comment_start)) == 0))
	{
		// Skip opening
		c += strlen(syntax->multiline_comment_start);
		(*len) += strlen(syntax->multiline_comment_start);
		
		while (*c)
		{
			// If it ends on the same line
			if (strncmp(c, syntax->multiline_comment_end, strlen(syntax->multiline_comment_end)) == 0)
			{
				(*len) += strlen(syntax->multiline_comment_end);
				return TK_COMMENT;
			}
			
			c++;
			(*len)++;
		}

		// If it is indeed, multi-line
		*multiline_end = syntax->multiline_comment_end;
		**last_token = TK_COMMENT;

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
		// Makefile
		else if (strcmp(syntax->filetype, "makefile") == 0)
		{
			// Shell invokations
			if (*c == '$' && *(c + 1) == '(')
			{
				c += 2;
				(*len) += 2;

				while (isalnum(*c) || *c == '_')
				{
					c++;
					(*len)++;
				}

				(*len)++;

				if (*c == ')') return TK_STRING;
			}
		}
		// Markdown
		else if (strcmp(syntax->filetype, "markdown") == 0)
		{
			if (*c == '#')
			{
				while (*c)
				{
					c++;
					(*len)++;
				}

				return TK_KEYWORD;
			}
			else if (*c == '!' || *c == '[')
			{
				if (*c == '!' && *(c + 1) != '[')
				{
					c++;
					(*len)++;
					return TK_NORMAL;
				}
				
				while (*c && *c != '(')
			    {
			        c++;
			        (*len)++;
			    }

				if (*c == '(') 
				{
					**last_token = TK_NUMBER;
					return TK_NUMBER;
				}
			}
			else if (**last_token == TK_NUMBER)
			{
				if (*c != '(')
				{
					**last_token = TK_NORMAL;
				}
				else
				{
					while (*c && *c != ')')
					{
						c++;
     					(*len)++;
					}

					if (*c == ')') 
					{
						c++;
     					(*len)++;
						return TK_STRING;
					}
				}
			}
		}
		
		// Default
		*len = 1;
		return TK_NORMAL;
	}
}

void syntax_hl_update(SYNTAX *syntax, BUFFER_NODE *node, char **multiline_end, char **last_token)
{
	node->h = realloc(node->h, node->rlen);
	
	int len, idx = 0;

	while (idx < node->rlen)
	{
		int token = tokenize(syntax, &(node->r)[idx], &len, multiline_end, last_token);

		memset(&(node->h)[idx], token, len);

		idx += len;
	}
}