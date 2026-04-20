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
#define TAB_STOP 4

void buf_render_line(BUFFER_NODE *buf_node)
{
	// Allocation for special chars rendering
	int len = buf_node->len;
	int tabs = 0;
	
	int i;
	for (i = 0; i < len; i++)
    	if (buf_node->s[i] == '\t') tabs++;
	
	free(buf_node->r);
	buf_node->r = malloc(len + tabs*(TAB_STOP - 1) + 1);
	
	int k = 0;
	for (i = 0; i < len; i++)
	{
		if (buf_node->s[i] == '\t')
		{
			buf_node->r[k++] = ' ';
      		while (k % TAB_STOP != 0) buf_node->r[k++] = ' ';
		}
		else
		{
			buf_node->r[k++] = buf_node->s[i];
		}
	}
	
	buf_node->r[k] = '\0';
	buf_node->rlen = k;
}

BUFFER_NODE *buf_add_new_line(char *s, int len)
{
	BUFFER_NODE *buf_node = malloc(sizeof(BUFFER_NODE));
	
	buf_node->s = s;
	buf_node->len = len;
	
	buf_node->r = NULL;
	buf_node->rlen = 0;
	
	buf_node->prev = NULL;
	buf_node->next = NULL;
	
	buf_render_line(buf_node);
	
	return buf_node;
}

BUFFER_CHAIN *buf_parse_file(const char *filepath)
{
	BUFFER_CHAIN *buf_chain = malloc(sizeof(BUFFER_CHAIN));
	buf_chain->head = NULL;
	buf_chain->lines_num = 0;
	buf_chain->cache_node = NULL;
	buf_chain->cache_line_num = 0;
	
	FILE *fp = fopen(filepath, "r");
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

BUFFER_CHAIN *buf_new_canvas()
{
	BUFFER_CHAIN *buf_chain = malloc(sizeof(BUFFER_CHAIN));
	
	char s[] = "";
	
	buf_chain->head = buf_add_new_line(s, 0);
	
	// Free line to start writing!!!
	buf_chain->lines_num = 1;
	
	buf_chain->cache_node = NULL;
	buf_chain->cache_line_num = 0;
	
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

void buf_insert(BUFFER_CHAIN *buf_chain, int line_num, int offset, char c)
{
	BUFFER_NODE *buf_node = buf_get_line_at(buf_chain, line_num, FALSE);
	
	buf_node->s = realloc(buf_node->s, buf_node->len + 1);
	
	memmove(&buf_node->s[offset + 1], &buf_node->s[offset], buf_node->len - offset + 1);
	
	buf_node->len++;
	
	buf_node->s[offset] = c;
	
	buf_render_line(buf_node);
}

void buf_remove(BUFFER_CHAIN *buf_chain, int line_num, int offset, int len)
{
	// TODO: Implement this!
	return;
}