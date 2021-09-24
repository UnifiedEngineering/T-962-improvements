#ifndef EEPROM_H_
#define EEPROM_H_

#include "stm32f1xx_hal.h"

void EEPROM_Init(I2C_HandleTypeDef* i2c);
void EEPROM_Dump(void);
int32_t EEPROM_Read(uint8_t* dest, uint32_t startpos, uint32_t len);
int32_t EEPROM_Write(uint32_t startdestpos, uint8_t* src, uint32_t len);

#endif /* EEPROM_H_ */
