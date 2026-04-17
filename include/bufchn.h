typedef struct BUFFER_NODE
{
	char *s;
	int len;
	int* prev;
	int* next;
} BUFFER_NODE;

typedef struct BUFFER_CHAIN
{
	BUFFER_NODE *head;
	int lines_num;
} BUFFER_CHAIN;

BUFFER_NODE *buf_add_new_line(char *s, int len);
BUFFER_CHAIN *buf_parse_file(const char *file_path);
BUFFER_NODE *buf_get_line_at(BUFFER_CHAIN *buf_chain, int line_num);
void buf_insert(BUFFER_CHAIN *buf_chain, int line_num, int offset, char *new);
void buf_remove(BUFFER_CHAIN *buf_chain, int line_num, int offset, int len);
