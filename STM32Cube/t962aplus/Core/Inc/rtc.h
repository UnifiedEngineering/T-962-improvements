#ifndef RTC_H_
#define RTC_H_

#include "main.h"
void RTC_Init(RTC_HandleTypeDef* rtc);
uint32_t RTC_Read(void);
void RTC_Zero(void);

#endif /* RTC_H_ */
