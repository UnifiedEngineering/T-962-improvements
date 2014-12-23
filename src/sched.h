#ifndef SCHED_H_
#define SCHED_H_

#include "t962.h" // For frequency constant

// This is the prescaler used to initialize the timer so we run a minimum of 6 times per us
// (for 55.296MHz 7 results in 7.899 ticks per us)
#define TIMER_PRESCALER (7)

//#define TICKS_PER_SECOND (PCLKFREQ / TIMER_PRESCALER)
//#define TICKS_PER_MS (PCLKFREQ / (TIMER_PRESCALER * 1000))
//#define TICKS_PER_US (PCLKFREQ / (TIMER_PRESCALER * 1000 * 1000))
#define TICKS_SECS(x) ((uint32_t)(((((double)x * (double)PCLKFREQ) / ((double)TIMER_PRESCALER))) + 0.5f))
#define TICKS_MS(x) ((uint32_t)(((((double)x * (double)PCLKFREQ) / ((double)TIMER_PRESCALER * 1000.0f))) + 0.5f))
#define TICKS_US(x) ((uint32_t)(((((double)x * (double)PCLKFREQ) / ((double)TIMER_PRESCALER * 1000.0f * 1000.0f))) + 0.5f))
#define TICKS_NS(x) ((uint32_t)(((((double)x * (double)PCLKFREQ) / ((double)TIMER_PRESCALER * 1000.0f * 1000.0f * 1000.0f))) + 0.5f))

typedef int32_t (*SchedCall_t)(void);
typedef enum eTask {
	SLEEP_WORK=0,
	BUZZER_WORK,
	KEYPAD_WORK,
	SYSFANPWM_WORK,
	MAIN_WORK,
	ADC_WORK,
	ONEWIRE_WORK,
	SPI_TC_WORK,
	UI_WORK,
	REFLOW_WORK,
	SYSFANSENSE_WORK,
	NV_WORK,
	SCHED_NUM_ITEMS // Last value
} Task_t;

void Sched_Init(void);
uint32_t Sched_GetTick(void);
void Sched_SetState(Task_t tasknum, uint8_t enable, int32_t future);
uint8_t Sched_IsOverride(void);
void Sched_SetWorkfunc(Task_t tasknum, SchedCall_t func);
int32_t Sched_Do(uint32_t fastforward);
void BusyWait( uint32_t numticks );

#endif /* SCHED_H_ */
