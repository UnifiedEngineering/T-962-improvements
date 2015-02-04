#ifndef SERIAL_H_
#define SERIAL_H_

void Serial_Init(void);

//non-blocking read
char uart_readc(void);

//non-blocking check
int uart_isrxready(void);

int uart_readline(char* buffer, int max_len);

#endif /* SERIAL_H_ */
