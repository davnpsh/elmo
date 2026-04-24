#include "terminal.h"
#include "helper.h"
#include "editor.h"

#define FALSE 0

int main(int argc, char **argv) 
{
	enable_raw_mode();
	init_editor();
	
	if (argc >= 2) 
	{
		editor_open(argv[1]);
	}
	
	while (1) 
	{
		editor_refresh_screen(FALSE);
		editor_process_keypress();
	}
	
	return 0;
}