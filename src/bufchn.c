#include "bufchn.h"
#include "helper.h"

BUFFER_CHAIN *parse_file(const char *file_path)
{
	BUFFER_CHAIN *buf_chain = malloc(sizeof(BUFFER_CHAIN));
	buf_chain->lines_num = 0;
	
	FILE *fp = fopen(file_path, "r");
	if (!fp) die("fopen");
	
	BUFFER_NODE *prev, *current;
	char *s = NULL;
	int len;
	
	while ((len = getline(&s, 0, fp)) != -1)
	{
		current = add_new_line(s, len);
		
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

BUFFER_NODE *add_new_line(char *s, int len)
{
	BUFFER_NODE *buf_node = malloc(sizeof(BUFFER_NODE));
	
	buf_node->s = s;
	buf_node->len = len;
	
	return buf_node;
}

BUFFER_NODE *get_line_at(BUFFER_CHAIN *buf_chain, int line_num)
{
	BUFFER_NODE *ptr = buf_chain->head;
	int current_line_num = 1;
	
	while (current_line_num != line_num)
	{
		if (ptr == NULL)
		{
			return NULL;
		}
		
		ptr = ptr->next;
	}
	
	return ptr;
}