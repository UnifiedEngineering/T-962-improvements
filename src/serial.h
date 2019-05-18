#ifndef SERIAL_H_
#define SERIAL_H_

#include "advancedcmd.h"
void Serial_Init(void);

//non-blocking read
char uart_readc(void);

//non-blocking check
int uart_isrxready(void);

//non-blocking check
void uart_rxflush(void);
unsigned uart_available(void);
uint8_t uart_chkAdvCmd(advancedSerialCMD* advCmd);
int uart_readline(char* buffer, int max_len);

#endif /* SERIAL_H_ */
