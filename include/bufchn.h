typedef struct BUFFER_CHAIN
{
	BUFFER_NODE *head;
	int lines_num;
} BUFFER_CHAIN;

typedef struct BUFFER_NODE
{
	char *s;
	int len;
	int* prev;
	int* next;
} BUFFER_NODE;


