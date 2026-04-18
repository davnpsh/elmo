#define _DEFAULT_SOURCE
#define _BSD_SOURCE
#define _GNU_SOURCE
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "bufchn.h"
#include "helper.h"

#define Bool int
#define TRUE 1
#define FALSE 0

BUFFER_NODE *buf_add_new_line(char *s, int len)
{
	BUFFER_NODE *buf_node = malloc(sizeof(BUFFER_NODE));
	
	buf_node->s = s;
	buf_node->len = len;
	buf_node->prev = NULL;
	buf_node->next = NULL;
	
	return buf_node;
}

BUFFER_CHAIN *buf_parse_file(const char *file_path)
{
	BUFFER_CHAIN *buf_chain = malloc(sizeof(BUFFER_CHAIN));
	buf_chain->head = NULL;
	buf_chain->lines_num = 0;
	buf_chain->cache_node = NULL;
	buf_chain->cache_line_num = 0;
	
	FILE *fp = fopen(file_path, "r");
	if (!fp) die("fopen");
	
	BUFFER_NODE *prev = NULL;
	BUFFER_NODE *current = NULL;
	
	char *s = NULL;
	ssize_t len;
	size_t linecap = 0;
	
	while ((len = getline(&s, &linecap, fp)) != -1)
	{
		while (len > 0 && (s[len - 1] == '\n' || s[len - 1] == '\r'))
			len--;
		
		char *copy = malloc(sizeof(char) * (len + 1));
		memcpy(copy, s, len);
		copy[len] = '\0';
		
		current = buf_add_new_line(copy, len);
		
		// Double-linked list relations:
		if (prev != NULL)
		{
			prev->next = current;
			current->prev = prev;
		}
		// Store the first line (pointer node):
		else
		{
			buf_chain->head = current;
		}
		
		buf_chain->lines_num++;
		prev = current;
	}
	
	fclose(fp);
	free(s);
	
	return buf_chain;
}

BUFFER_NODE *buf_get_line_at(BUFFER_CHAIN *buf_chain, int line_num, Bool cache)
{
	BUFFER_NODE *ptr;
	int current_line_num;
	
	if ((buf_chain == NULL) 
		|| (buf_chain->head == NULL)) return NULL;
	
	if (line_num > buf_chain->lines_num) return NULL;
	
	// Try to fetch from cache
	if ((buf_chain->cache_node != NULL)
	 && (abs(line_num - buf_chain->cache_line_num) < line_num))
	{
		ptr = buf_chain->cache_node;
		current_line_num = buf_chain->cache_line_num;
		
		// Backward search
		if (current_line_num > line_num)
		{
			while (current_line_num != line_num)
			{
				ptr = ptr->prev;
				current_line_num--;
			}
		}
	}
	// Fetch from Buffer Chain head instead
	else
	{
		ptr = buf_chain->head;
		current_line_num = 1;
	}
	
	// Forward search
	while (current_line_num != line_num)
	{
		ptr = ptr->next;
		current_line_num++;
	}
	
	if (cache)
	{
		buf_chain->cache_node = ptr;
		buf_chain->cache_line_num = current_line_num;
	}
	
	return ptr;
}

void buf_insert(BUFFER_CHAIN *buf_chain, int line_num, int offset, char *new)
{
	BUFFER_NODE *buf_node = buf_get_line_at(buf_chain, line_num, FALSE);
	
	if (buf_node == NULL) return;
	
	if (offset > buf_node->len) return;
	
	BUFFER_NODE *prev = buf_node;
	BUFFER_NODE *last = (BUFFER_NODE *)buf_node->next;
	
	for (int i = 0; *new != '\0'; i++, new++)
	{
		// TODO: Implement this!
	}
}

void buf_remove(BUFFER_CHAIN *buf_chain, int line_num, int offset, int len)
{
	// TODO: Implement this!
	return;
}