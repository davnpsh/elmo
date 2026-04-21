#define Bool int

typedef struct BUFFER_NODE
{
	char *s;	// Actual string
	int len;
	char *r;	// Render
	int rlen;
	struct BUFFER_NODE *prev;
	struct BUFFER_NODE *next;
} BUFFER_NODE;

typedef struct BUFFER_CHAIN
{
	BUFFER_NODE *head;
	int lines_num;
	// Cache
	BUFFER_NODE *cache_node;
	int cache_line_num;
} BUFFER_CHAIN;

/**
 * Renders the characters of the buffer to something fancy.
 * @param BUFFER_CHAIN *buf_chain.
 */
void buf_render_line(BUFFER_NODE *buf_node);

/**
 * Adds a new hanging line waiting to be inserted into a buffer.
 * @param char *s Pointer to the line.
 * @param int len Length of the line.
 */
BUFFER_NODE *buf_add_new_line(char *s, int len);

/**
 * Parses a text file into a Buffer Chain.
 * @param const char *filepath Text file path.
 * @return A pointer to a Buffer Chain.
 */
BUFFER_CHAIN *buf_parse_file(const char *filepath);

/**
 * Produces a new Buffer Chain.
 * @return A pointer to a Buffer Chain.
 */
BUFFER_CHAIN *buf_new_canvas();

/**
 * Retrieves a node of the Buffer Chain corresponding to a line in buffer.
 * @param BUFFER_CHAIN *buf_chain.
 * @param int line_num The number of the line.
 * @param Bool cache Whether store cache or not.
 * @return The node representing the line.
 */
BUFFER_NODE *buf_get_line_at(BUFFER_CHAIN *buf_chain, int line_num, Bool cache);

/**
 * Inserts new text into the Buffer Chain.
 * @param BUFFER_CHAIN *buf_chain.
 * @param int line_num The number of the line.
 * @param int offset Index to start the insert operation from.
 * @param char *c New char to add to the buffer.
 */
void buf_insert(BUFFER_CHAIN *buf_chain, int line_num, int offset, char c);

/**
 * Deletes text from the Buffer Chain.
 * @param BUFFER_CHAIN *buf_chain.
 * @param int line_num The number of the line.
 * @param int offset Index to start the insert operation from.
 * @param len Quantity of characters to delete from the buffer.
 */
void buf_remove(BUFFER_CHAIN *buf_chain, int line_num, int offset, int len);

/**
 * Produces a single string ready to be written into a file.
 * @param BUFFER_CHAIN *buf_chain.
 * @param int *len A reference to the size of the string.
 * @return The string.
 */
char *buf_read(BUFFER_CHAIN *buf_chain, int *len);

/**
 * Writes a buffer chain into a file.
 * @param BUFFER_CHAIN *buf_chain.
 * @param const char *filepath Text file path.
 * @return Status code.
 */
int buf_save(BUFFER_CHAIN *buf_chain, const char *filepath);