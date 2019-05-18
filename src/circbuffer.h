#ifndef CIRCBUFFER_H_
#define CIRCBUFFER_H_

/* Size of buffer.
 * The putchar() function used here will block when the buffer is full (buffer
 * is drained in interrupt), so if you want to continously write lots of data
 * increase the buffer size. Change putchar() to non-blocking and read the
 * 'dropped' parameter in the buffer structure to determine how your buffer
 * size is doing.
 */
#define CIRCBUFSIZE 256

typedef struct {
	volatile unsigned int head;
	volatile unsigned int tail;
	volatile unsigned int dropped;
	char buf[CIRCBUFSIZE];
} tcirc_buf;

void init_circ_buf(tcirc_buf * cbuf);
void add_to_circ_buf(tcirc_buf *cbuf, char ch, int block);
int circ_buf_has_char(tcirc_buf *cbuf);
void circ_buf_flush(tcirc_buf *cbuf);
char circ_buf_peek(tcirc_buf * cbuf, int offset);
char* circ_buf_getAddr(tcirc_buf * cbuf, int offset);
char get_from_circ_buf(tcirc_buf *cbuf);
unsigned int circ_buf_count(tcirc_buf *cbuf);

#endif  /* CIRCBUFFER_H_ */
