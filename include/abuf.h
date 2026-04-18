typedef struct APPEND_BUFFER
{
	char *b;
	int len;
} APPEND_BUFFER;

void ab_append(APPEND_BUFFER *ab, const char *s, int len);
void ab_free(APPEND_BUFFER *ab);

