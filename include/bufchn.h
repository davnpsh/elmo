typedef struct BUFFER_NODE
{
	char *s;
	int len;
	struct BUFFER_NODE *prev;
	struct BUFFER_NODE *next;
} BUFFER_NODE;

typedef struct BUFFER_CHAIN
{
	BUFFER_NODE *head;
	int lines_num;
} BUFFER_CHAIN;

/**
 * Adds a new hanging line waiting to be inserted into a buffer.
 * @param char *s Contents of the line.
 * @param int len Length of the line.
 */
BUFFER_NODE *buf_add_new_line(char *s, int len);

/**
 * Parses a text file into a Buffer Chain.
 * @param const char *s Text file path.
 * @return A pointer to a Buffer Chain.
 */
BUFFER_CHAIN *buf_parse_file(const char *file_path);

/**
 * Retrieves a node of the Buffer Chain corresponding to a line in buffer.
 * @param BUFFER_CHAIN *buf_chain.
 * @param int line_num The number of the line.
 * @return The node representing the line.
 */
BUFFER_NODE *buf_get_line_at(BUFFER_CHAIN *buf_chain, int line_num);

/**
 * Inserts new text into the Buffer Chain.
 * @param BUFFER_CHAIN *buf_chain.
 * @param int line_num The number of the line.
 * @param int offset Index to start the insert operation from.
 * @param char *new New contents to add to the buffer.
 */
void buf_insert(BUFFER_CHAIN *buf_chain, int line_num, int offset, char *new);

/**
 * Deletes text from the Buffer Chain.
 * @param BUFFER_CHAIN *buf_chain.
 * @param int line_num The number of the line.
 * @param int offset Index to start the insert operation from.
 * @param len Quantity of characters to delete from the buffer.
 */
void buf_remove(BUFFER_CHAIN *buf_chain, int line_num, int offset, int len);
