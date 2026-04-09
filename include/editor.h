typedef struct EDITOR_CONFIG
{
	int screenrows;
	int screencols;
	struct termios orig_termios;
} EDITOR_CONFIG;