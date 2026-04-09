#include "terminal.h"
#include "helper.h"
#include "editor.h"

int main() 
{
	enable_raw_mode();
	init_editor();
	
	while (1) 
	{
		editor_refresh_screen();
		editor_process_keypress();
	}
	
	return 0;
}