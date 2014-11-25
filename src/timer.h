#ifndef TIMER_H_
#define TIMER_H_

void Timer_Init( void );
void BusyWait8( uint32_t numusecstimes8 ); // 8 will delay for 1 usec
uint32_t Timer_Get(void);

#endif /* TIMER_H_ */
