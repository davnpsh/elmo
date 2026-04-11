typedef struct ABUF
{
	char *b;
	int len;
} ABUF;

void abAppend(ABUF *ab, const char *s, int len);
void abFree(ABUF *ab);

