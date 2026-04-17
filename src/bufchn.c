#define _DEFAULT_SOURCE
#define _BSD_SOURCE
#define _GNU_SOURCE
#include <stdlib.h>
#include <stdio.h>

#include "bufchn.h"
#include "helper.h"

BUFFER_NODE *buf_add_new_line(char *s, int len)
{
	BUFFER_NODE *buf_node = malloc(sizeof(BUFFER_NODE));
	
	buf_node->s = s;
	buf_node->len = len;
	
	return buf_node;
}

BUFFER_CHAIN *buf_parse_file(const char *file_path)
{
	BUFFER_CHAIN *buf_chain = malloc(sizeof(BUFFER_CHAIN));
	buf_chain->lines_num = 0;
	
	FILE *fp = fopen(file_path, "r");
	if (!fp) die("fopen");
	
	BUFFER_NODE *prev, *current;
	char *s = NULL;
	int len;
	size_t linecap = 0;
	
	while ((len = getline(&s, &linecap, fp)) != -1)
	{
		current = buf_add_new_line(s, len);
		
		if (prev != NULL)
		{
			prev->next = (int *)current;
			current->prev = (int *)prev;
		}
		// First node
		else
		{
			buf_chain->head = current;
		}
		
		buf_chain->lines_num++;
		prev = current;
	}
	
	return buf_chain;
}

BUFFER_NODE *buf_get_line_at(BUFFER_CHAIN *buf_chain, int line_num)
{
	BUFFER_NODE *ptr = buf_chain->head;
	int current_line_num = 1;
	
	while (current_line_num != line_num)
	{
		if (ptr == NULL)
		{
			return NULL;
		}
		
		ptr = (BUFFER_NODE *)ptr->next;
	}
	
	return ptr;
}

void buf_insert(BUFFER_CHAIN *buf_chain, int line_num, int offset, char *new)
{
	BUFFER_NODE *buf_node = buf_get_line_at(buf_chain, line_num);
	
	if (buf_node == NULL) return;
	
	if (offset > buf_node->len) return;
	
	BUFFER_NODE *prev = buf_node;
	BUFFER_NODE *last = buf_node->next;
	
	for (int i = 0; *new != '\0'; i++, new++)
	{
		// TODO: Implement this!
	}
}

// void buf_remove(BUFFER_CHAIN *buf_chain, int line_num, int offset, int len)
// {
// 	return;
// }