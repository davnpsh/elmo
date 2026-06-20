#ifndef COMMAND_H
#define COMMAND_H

typedef void (*cmd_handler)(char *args);

typedef struct COMMAND
{
	char *name;
	char *alias;
	cmd_handler handler;
} COMMAND;

extern COMMAND commands[];

#define COMMANDS_ENTRIES (sizeof(commands) / sizeof(commands[0]))

void process_command(char* command);

#endif	/* COMMAND_H */