typedef enum TOKEN
{
	TK_NORMAL = 0,
	TK_NUMBER,
	TK_STRING,
	TK_IDENTIFIER,
	TK_KEYWORD,
	TK_EOF
} TOKEN;

TOKEN tokenize(char *s, int *len);