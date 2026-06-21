#ifndef BUFCHN_H
#define BUFCHN_H

typedef int Bool;
#define TRUE 1
#define FALSE 0

typedef struct BUFFER_NODE
{
	char *s;	// Actual string
	int len;
	char *r;	// Render
	int rlen;
	char *h;	// Highlight map
	struct BUFFER_NODE *prev;
	struct BUFFER_NODE *next;

	// Render cache
	int display_row_offset;
	int display_wrap_rows;

	// Highlight multiline state
	// (comment/string)
	char hl_multiline_state;
} BUFFER_NODE;

typedef struct BUFFER_CHAIN
{
	BUFFER_NODE *head;
	int lines_num;
	void *syntax;

	// Node cache
	BUFFER_NODE *cache_node;
	int cache_line_num;

	// Render cache
	int total_display_rows;
	Bool update_layout;
	int editor_width_cache;
} BUFFER_CHAIN;

/**
 * Deallocates a Buffer Node from memory.
 * @param BUFFER_NODE *buf_node
 */
void buf_free_node(BUFFER_NODE *buf_node);

/**
 * Deallocates a Buffer Chain from memory.
 * @param BUFFER_CHAIN *buf_chain
 */
void buf_free_chain(BUFFER_CHAIN *buf_chain);

/**
 * Renders the characters of the buffer to something fancy.
 * @param BUFFER_NODE *buf_node
 */
void buf_render_line(BUFFER_NODE *buf_node);

/**
 * Allocates a new BUFFER_NODE wrapping the given string.
 * @param char *s Pointer to the line
 * @param int len Length of the line
 */
BUFFER_NODE *buf_add_new_line(char *s, int len);

/**
 * Parses a text file into a Buffer Chain.
 * @param const char *filepath Text file path
 * @return A pointer to a Buffer Chain
 */
BUFFER_CHAIN *buf_parse_file(char *filepath);

/**
 * Produces a new Buffer Chain.
 * @return A pointer to a Buffer Chain
 */
BUFFER_CHAIN *buf_new_chain();

/**
 * Retrieves a node of the Buffer Chain corresponding to a line in buffer.
 * @param BUFFER_CHAIN *buf_chain
 * @param int line_num The number of the line
 * @param Bool cache Whether to cache the result for subsequent lookups
 * @return The node representing the line
 */
BUFFER_NODE *buf_get_line_at(BUFFER_CHAIN *buf_chain, int line_num, Bool cache);

/**
 * Inserts new text into the Buffer Chain.
 * @param BUFFER_CHAIN *buf_chain
 * @param int line_num The number of the line
 * @param int offset Index to start the insert operation from
 * @param char c New char to add to the buffer
 */
void buf_insert(BUFFER_CHAIN *buf_chain, int line_num, int offset, char c);

/**
 * Deletes text from the Buffer Chain.
 * @param BUFFER_CHAIN *buf_chain
 * @param int line_num The number of the line
 * @param int offset Index to start the delete operation from
 */
void buf_delete(BUFFER_CHAIN *buf_chain, int line_num, int offset);

/**
 * Produces a single string ready to be written into a file.
 * @param BUFFER_CHAIN *buf_chain
 * @param int *len Output parameter; receives the total length of the produced string
 * @return The string
 */
char *buf_read(BUFFER_CHAIN *buf_chain, int *len);

/**
 * Writes a buffer chain into a file.
 * @param BUFFER_CHAIN *buf_chain
 * @param const char *filepath Text file path
 * @return 0 on success, errno on failure
 */
int buf_save(BUFFER_CHAIN *buf_chain, char *filepath);

/**
 * Toggles a line comment.
 * @param BUFFER_CHAIN *buf_chain
 * @param int line_num The number of the line
 */
void buf_toggle_comment_line(BUFFER_CHAIN *buf_chain, int line_num);

#endif /* BUFCHN_H */