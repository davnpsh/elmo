#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "command.h"
#include "editor.h"
#include "syntax.h"
#include "bufchn.h"

void cmd_save(char* args)
{
	if (args != NULL)
	{
		free(editor.filepath);
		editor.filepath = malloc(strlen(args) + 1);
		strcpy(editor.filepath, args);
	}
	
	editor_save();
}

void cmd_quit(char* args)
{
	(void)args;

	editor_cleanup();
	
	write(STDOUT_FILENO, "\x1b[2J", 4);
	write(STDOUT_FILENO, "\x1b[H", 3);
	exit(0);
}

void cmd_quit_safe(char* args)
{
	(void)args;
	
	if (editor.dirty)
		editor_set_status_msg("use /quit! to exit without saving.");
	else cmd_quit(NULL);
}

void cmd_jump(char* args)
{
	if (args != NULL)
		editor_jump(atoi(args));
}

void cmd_linenumbers(char* args)
{
	if (args == NULL)
		editor.show_line_num_gutter = !editor.show_line_num_gutter;
	else if (strcmp(args, "rel") == 0)
		editor.line_num_mode = RELATIVE;
	else if (strcmp(args, "abs") == 0)
		editor.line_num_mode = ABSOLUTE;
}

void cmd_syntax(char* args)
{
	if (strcmp(args, "off") == 0)
	{
		editor.buf_chain->syntax = NULL;
		syntax_hl_update_buf(editor.buf_chain);
	}
	else if (args != NULL)
	{
		SYNTAX *syntax = get_syntax_by_filetype(args);

		if (syntax == NULL) 
		{
			editor_set_status_msg("invalid filetype!");
			return;
		}

		editor.buf_chain->syntax = syntax;
		syntax_hl_update_buf(editor.buf_chain);
	}
}

COMMAND commands[] = {
	{ "quit", "q",  cmd_quit_safe },
	{ "quit!", "q!",  cmd_quit },
	{ "save", "s",  cmd_save },
	{ "jump", "j",  cmd_jump },
	{ "linenumbers", "nums",  cmd_linenumbers },
	{ "syntax", NULL,  cmd_syntax },
};

void process_command(char* command)
{
	// Parsing the command
	command++; //	delete '/'

	char *name = strtok(command, " ");
    char *args = strtok(NULL, "");

    for (unsigned int i = 0; i < COMMANDS_ENTRIES; i++)
    {
    	if (strcmp(name, commands[i].name) == 0
            || (commands[i].alias && strcmp(name, commands[i].alias) == 0))
        {
            commands[i].handler(args);
            return;
        }
    }

    editor_set_status_msg("invalid command!");
}