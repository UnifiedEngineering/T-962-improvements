#ifndef RTC_H_
#define RTC_H_

void RTC_Init(void);
uint32_t RTC_Read(void);
void RTC_Zero(void);
void RTC_Hold(void);
void RTC_Resume(void);

#endif /* RTC_H_ */
