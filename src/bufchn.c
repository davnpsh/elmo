#define _DEFAULT_SOURCE

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include <ctype.h>

#include "bufchn.h"
#include "render.h"
#include "syntax.h"
#include "util.h"

void buf_invalidate_cache(BUFFER_CHAIN* buf_chain)
{
	buf_chain->cache_node = NULL;
    buf_chain->cache_line_num = 0;
}

void buf_free_node(BUFFER_NODE *buf_node)
{
	free(buf_node->s);
	free(buf_node->r);
	free(buf_node->h);
	free(buf_node);
}

void buf_free_chain(BUFFER_CHAIN *buf_chain)
{
	if (buf_chain == NULL) return;

	BUFFER_NODE *current = buf_chain->head;
	
    while (current)
    {
    	BUFFER_NODE *next = current->next;
        buf_free_node(current);
        current = next;
    }

    free(buf_chain);
}

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

	buf_node->h = NULL;
	
	buf_node->prev = NULL;
	buf_node->next = NULL;

	buf_node->display_row_offset = 0;
	buf_node->display_wrap_rows = 0;
	
	buf_node->hl_multiline_state = -1;
	
	return buf_node;
}

BUFFER_CHAIN *buf_parse_file(char *filepath)
{
	BUFFER_CHAIN *buf_chain = malloc(sizeof(BUFFER_CHAIN));
	buf_chain->head = NULL;
	buf_chain->lines_num = 0;
	buf_chain->cache_node = NULL;
	buf_chain->cache_line_num = 0;
	buf_chain->syntax = get_syntax_by_filematch(filepath);
	buf_chain->total_display_rows = 0;
	buf_chain->update_layout = FALSE;
	buf_chain->editor_width_cache = 0;
	
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
		buf_render_line(current);
		
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
	
	// Reading empty file (0 bytes)
	if (buf_chain->lines_num == 0)
	{
		char *s = malloc(1);
		s[0] = '\0';

		buf_chain->head = buf_add_new_line(s, 0);
		buf_render_line(buf_chain->head);
		
		// Free line to start writing!!!
		buf_chain->lines_num = 1;
	}
	
	fclose(fp);
	free(s);

	syntax_hl_update_buf(buf_chain);
	
	return buf_chain;
}

BUFFER_CHAIN *buf_new_chain()
{
	BUFFER_CHAIN *buf_chain = malloc(sizeof(BUFFER_CHAIN));
	
	char *s = malloc(1);
	s[0] = '\0';
	
	buf_chain->head = buf_add_new_line(s, 0);
	buf_render_line(buf_chain->head);
	
	// Free line to start writing!!!
	buf_chain->lines_num = 1;
	
	buf_chain->cache_node = NULL;
	buf_chain->cache_line_num = 0;

	buf_chain->syntax = NULL;

	syntax_hl_update_buf(buf_chain);

	buf_chain->total_display_rows = 0;
	buf_chain->update_layout = FALSE;
	buf_chain->editor_width_cache = 0;
	
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
	
	if (c == '\r')
	{
		int len = buf_node->len - offset;
		
		char *copy = malloc(sizeof(char) * (len + 1));
		
		memcpy(copy, &buf_node->s[offset], len);
		
		copy[len] = '\0';
		
		BUFFER_NODE *new = buf_add_new_line(copy, len);
		buf_render_line(new);
		
		// Fix relations
		new->prev = buf_node;
		new->next = buf_node->next;
		
		if (buf_node->next)
			buf_node->next->prev = new;
		
		buf_node->next = new;
		
		buf_node->len = offset;
		
		buf_node->s[offset] = '\0';
		
		buf_node->s = realloc(buf_node->s, buf_node->len + 1);
		
		buf_render_line(buf_node);
		
		buf_chain->lines_num++;

		buf_invalidate_cache(buf_chain);

		syntax_hl_update_region(buf_chain, line_num);
		syntax_hl_update_region(buf_chain, line_num + 1);
	}
	else
	{
		char *tmp = realloc(buf_node->s, buf_node->len + 2);
		if (tmp) buf_node->s = tmp;
		
		memmove(&buf_node->s[offset + 1], &buf_node->s[offset], buf_node->len - offset + 1);
		
		buf_node->len++;
		
		buf_node->s[offset] = c;
		
		buf_render_line(buf_node);

		syntax_hl_update_region(buf_chain, line_num);
	}
}

void buf_delete(BUFFER_CHAIN *buf_chain, int line_num, int offset)
{
	if (offset == 0 && line_num == 1) return;
	
	BUFFER_NODE *buf_node = buf_get_line_at(buf_chain, line_num, FALSE);
	
	if (offset > 0)
	{
		memmove(&buf_node->s[offset - 1], &buf_node->s[offset], buf_node->len - offset);
		
		buf_node->len--;
		
		buf_node->s[buf_node->len] = '\0';
		
		buf_render_line(buf_node);

		syntax_hl_update_region(buf_chain, line_num);
	}
	else
	{
		BUFFER_NODE *prev_node = buf_node->prev;
		
		prev_node->s = realloc(prev_node->s, prev_node->len + buf_node->len + 1);
		
		memcpy(&prev_node->s[prev_node->len], buf_node->s, buf_node->len);
		
		prev_node->len += buf_node->len;
		
		prev_node->s[prev_node->len] = '\0';
		
		buf_render_line(prev_node);
		
		prev_node->next = buf_node->next;
		
		if (buf_node->next)
			buf_node->next->prev = prev_node;
		
		buf_free_node(buf_node);
		
		buf_chain->lines_num--;

		buf_invalidate_cache(buf_chain);

		syntax_hl_update_region(buf_chain, line_num - 1);
		syntax_hl_update_region(buf_chain, line_num);
	}
}

char *buf_read(BUFFER_CHAIN *buf_chain, int *len)
{
	BUFFER_NODE *ptr;
	
	*len = 0;
	ptr = buf_chain->head;
	while (ptr)
	{
		*len += ptr->len + 1;
		ptr = ptr->next;
	}
	
	char *buf = malloc(*len);
	char *p = buf; 
	ptr = buf_chain->head;
	while (ptr)
	{
		memcpy(p, ptr->s, ptr->len);
	    p += ptr->len;
	    *p = '\n';
	    p++;
		ptr = ptr->next;
	}
	
	return buf;
}

int buf_save(BUFFER_CHAIN *buf_chain, char *filepath)
{
	int len;
	char *buf = buf_read(buf_chain, &len);
	
	int fd = open(filepath, O_RDWR | O_CREAT, 0644);
	
	if (fd != -1) 
	{
		if (ftruncate(fd, len) != -1) 
		{
			if (write(fd, buf, len) == len) 
			{
				close(fd);
				free(buf);

				buf_chain->syntax = get_syntax_by_filematch(filepath);
				
				return 0;
			}
		}
		close(fd);
	}
	
	free(buf);
	
	return errno;
}

int buf_toggle_comment_line(BUFFER_CHAIN *buf_chain, int line_num, int *shift)
{
	if (buf_chain == NULL || buf_chain->syntax == NULL) return -1;

	char *comment_pattern = ((SYNTAX *)buf_chain->syntax)->singleline_comment;
	if (comment_pattern == NULL) return -1;

	size_t comment_pattern_len = strlen(comment_pattern);
	*shift = comment_pattern_len + 1;

	BUFFER_NODE *line = buf_get_line_at(buf_chain, line_num, FALSE);

	int offset = 0;
	while (offset < line->len && (line->s[offset] == '\t' || line->s[offset] == ' '))
		offset++;

	Bool is_commented = strncmp(&line->s[offset], comment_pattern, comment_pattern_len) == 0;

	// Uncomment
	if (is_commented)
	{
		size_t k;
		for (k = 0; k < comment_pattern_len; k++)
			buf_delete(buf_chain, line_num, 1 + offset);

		if (line->s[offset] == ' ')
			buf_delete(buf_chain, line_num, 1 + offset);
		else 
			(*shift)--;

		return 1;
	}
	// Comment
	else
	{
		size_t k;
		for (k = 0; k < comment_pattern_len; k++)
			buf_insert(buf_chain, line_num, k + offset, comment_pattern[k]);

		buf_insert(buf_chain, line_num, k + offset, ' ');

		return 0;
	}
}
