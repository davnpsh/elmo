#ifndef BUFCHN_H
#define BUFCHN_H

typedef struct POSITION POSITION;

typedef int Bool;
#define TRUE 1
#define FALSE 0

/**
 * Represents a single line in the buffer as a node in a doubly-linked list.
 */
typedef struct BUFFER_NODE
{
	char *s;	// Raw string (as stored on disk)
	int len;
	char *r;	// Rendered string (tabs expanded, etc.)
	int rlen;
	char *h;	// Highlight map: one token type byte per character in r
	
	struct BUFFER_NODE *prev;
	struct BUFFER_NODE *next;

	// --- Render cache
	int display_row_offset;		// Absolute display row where this line starts
	int display_wrap_rows;		// Number of display rows this line occupies when wrapped

	// --- Highlight state
	char hl_multiline_state;	// State of the line (part of multi-comment/string block)
								// represented by a token type
} BUFFER_NODE;

/**
 * Holds the full document as a doubly-linked list of BUFFER_NODEs.
 */
typedef struct BUFFER_CHAIN
{
	BUFFER_NODE *head;	// First line of the document
	int lines_num;		// Total number of logical lines
	void *syntax;		// Syntax rules (C, markdown, etc.)

	// --- Node cache
	BUFFER_NODE *cache_node;	// Last node cached
	int cache_line_num;			// ...and its line number

	// --- Render cache
	int total_display_rows;		// Total number of display lines
	Bool update_layout;			// Whether to update the layout (display rows and offsets)
	int editor_width_cache;		// Editor width used for the last layout computation
} BUFFER_CHAIN;

/**
 * Deallocates a BUFFER_NODE from memory.
 * 
 * @param buf_node  Node to free.
 */
void buf_free_node(BUFFER_NODE *buf_node);

/**
 * Deallocates an entire BUFFER_CHAIN and all its nodes.
 * 
 * @param buf_chain  Chain to free.
 */
void buf_free_chain(BUFFER_CHAIN *buf_chain);

/**
 * Builds the rendered string (r, rlen) for a node by expanding tabs
 * and any other display transformations.
 * 
 * @param buf_node  Node to render.
 */
void buf_render_line(BUFFER_NODE *buf_node);

/** 
 * Allocates a new BUFFER_NODE wrapping the given string.
 *
 * @param s    Heap-allocated string for this line.
 * @param len  Length of s, excluding null terminator.
 * @return     Newly allocated node.
 */
BUFFER_NODE *buf_add_new_line(char *s, int len);

/**
 * Parses a text file into a new BUFFER_CHAIN, one node per line.
 * 
 * @param filepath  Path to the file to open.
 * @return          Newly allocated chain.
 */
BUFFER_CHAIN *buf_parse_file(char *filepath);

/**
 * Allocates a new empty BUFFER_CHAIN with a single blank line.
 *
 * @return  Newly allocated chain.
 */
BUFFER_CHAIN *buf_new_chain();

/**
 * Retrieves the BUFFER_NODE at the given 1-based line number.
 * Optionally caches the result to speed up nearby subsequent lookups.
 *
 * @param buf_chain  Chain to search.
 * @param line_num   1-based line number. Must be in [1, lines_num].
 * @param cache      If TRUE, stores the result in the chain's lookup cache.
 * @return           The node at line_num.
 */
BUFFER_NODE *buf_get_line_at(BUFFER_CHAIN *buf_chain, int line_num, Bool cache);

/**
 * Inserts a single character into the buffer at the given position.
 * '\r' splits the line at offset.
 * Automatically re-renders the affected line(s) and updates syntax highlighting.
 *
 * @param buf_chain  Chain to modify.
 * @param line_num   1-based line number.
 * @param offset     0-based character offset within the line.
 * @param c          Character to insert, or '\r' to insert a line break.
 */
void buf_insert(BUFFER_CHAIN *buf_chain, int line_num, int offset, char c);

/**
 * Replaces a block of selected text with a single character.
 *
 * @param buf_chain     Chain to modify.
 * @param select_start  Logical start of selection (x = char offset, y = 0-based line).
 * @param select_end    Logical end of selection (x = char offset, y = 0-based line).
 * @param c             Character to insert after the deletion.
 */
void buf_insert_block(BUFFER_CHAIN *buf_chain, POSITION select_start, POSITION select_end, char c);

/**
 * Deletes a single character from the buffer.
 * Automatically re-renders the affected line(s) and updates syntax highlighting.
 *
 * @param buf_chain  Chain to modify.
 * @param line_num   1-based line number.
 * @param offset     0-based character offset. If 0, merges with previous line.
 */
void buf_delete(BUFFER_CHAIN *buf_chain, int line_num, int offset);

/**
 * Deletes a block of selected text.
 * Automatically re-renders the affected line(s) and updates syntax highlighting.
 *
 * @param buf_chain     Chain to modify.
 * @param select_start  Logical start of selection (x = char offset, y = 0-based line).
 * @param select_end    Logical end of selection (x = char offset, y = 0-based line).
 */
void buf_delete_block(BUFFER_CHAIN *buf_chain, POSITION select_start, POSITION select_end);

/**
 * Serializes the entire buffer into a single heap-allocated string,
 * with newlines between lines.
 *
 * @param buf_chain  Chain to serialize.
 * @param len        Output: receives the total byte count.
 * @return           Heap-allocated string containing the full document.
 */
char *buf_read_document(BUFFER_CHAIN *buf_chain, int *len);

/**
 * Serializes a single line into a single heap-allocated string.
 *
 * @param buf_chain  Chain to serialize.
 * @param line_num   1-based line number.
 * @return           Heap-allocated string containing the line.
 */
char *buf_read_line(BUFFER_CHAIN *buf_chain, int line_num);

/**
 * Serializes a text selection into a single heap-allocated string.
 *
 * @param buf_chain  	Chain to serialize.
 * @param select_start  Logical start of selection (x = char offset, y = 0-based line).
 * @param select_end    Logical end of selection (x = char offset, y = 0-based line).
 * @return           	Heap-allocated string containing the selection.
 */
char *buf_read_selection(BUFFER_CHAIN *buf_chain, POSITION select_start, POSITION select_end);

/**
 * Writes the buffer contents to a file, overwriting it if it exists.
 *
 * @param buf_chain  Chain to write.
 * @param filepath   Destination file path.
 * @return           0 on success, errno value on failure.
 */
int buf_save(BUFFER_CHAIN *buf_chain, char *filepath);

/**
 * Toggles a single-line comment on the given line.
 *
 * @param buf_chain  Chain to modify.
 * @param line_num   1-based line number.
 * @param shift      Output: number of characters inserted or removed
 * 					 (according to the single-line comment marker length).
 * @return           0 on comment, 1 on uncomment, -1 on failure.
 */
int buf_toggle_comment_line(BUFFER_CHAIN *buf_chain, int line_num, int *shift);

/**
 * Toggles comments on a block of selected text.
 *
 * @param buf_chain        Chain to modify.
 * @param line_num_start   1-based first line of the block.
 * @param line_num_end     1-based last line of the block.
 * @param shift_first_line Output: number of characters inserted or removed
 * 						   in the first line (according to the single-line 
 * 						   comment marker length).
 * @param shift_last_line  Output: number of characters inserted or removed
 * 						   in the last line (according to the single-line 
 * 						   comment marker length).
 * @return                 0 on comment, 1 on uncomment, -1 on failure.
 */
int buf_toggle_comment_block(BUFFER_CHAIN *buf_chain, int line_num_start, int line_num_end, int *shift_first_line, int *shift_last_line);

#endif /* BUFCHN_H */