typedef struct BUFFER_CHAIN BUFFER_CHAIN;

void die(const char *s);
int get_line_display_rows(int line_len, int width);
int get_total_display_rows(BUFFER_CHAIN *buf_chain, int width);
void render_coords(int *rx, int *ry, int x, int y, BUFFER_CHAIN *buf_chain, int width);
void get_offset_coordinates(int *row_offset, int *wrap_offset, int ry, BUFFER_CHAIN *buf_chain, int width);
int token_to_color(unsigned char token);
int digit_count(int n);