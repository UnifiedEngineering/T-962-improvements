#ifndef BUZZER_H_
#define BUZZER_H_

#include <stdint.h>

typedef enum eBuzzFreq {
	BUZZ_NONE = 0,
	BUZZ_2KHZ = 3,
	BUZZ_1KHZ = 4,
	BUZZ_500HZ = 5,
	BUZZ_250HZ = 6
} BuzzFreq_t;

void Buzzer_Init(void);
void Buzzer_Beep(BuzzFreq_t freq, uint8_t volume, int32_t ticks);

#endif /* BUZZER_H_ */
