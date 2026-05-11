typedef struct SYNTAX 
{
	char *filetype;
	char **filematch;
	char **keywords;
	char *singleline_comment;
	char *multiline_comment_start;
	char *multiline_comment_end;
	int flags;
} SYNTAX;

#define HL_HIGHLIGHT_NUMBERS (1<<0)
#define HL_HIGHLIGHT_STRINGS (1<<1)

extern char *C_extensions[];
extern char *C_keywords[];
extern SYNTAX syntax_db[];

#define SYNTAX_ENTRIES (sizeof(syntax_db) / sizeof(syntax_db[0]))

enum TOKEN
{
	TK_NORMAL = 0,
	TK_NUMBER,
	TK_STRING,
	TK_IDENTIFIER,
	TK_KEYWORD,
	TK_EOF
};

SYNTAX *get_syntax(const char *filepath);
char tokenize(SYNTAX *syntax, char *s, int *len);
void syntax_hl_update(SYNTAX *syntax, char **h, char **r, int rlen);
