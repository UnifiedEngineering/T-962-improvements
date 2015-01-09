#ifndef MAX31855_H_
#define MAX31855_H_

uint32_t SPI_TC_Init(void);
int SPI_IsTCPresent(uint8_t tcid);
float SPI_GetTCReading(uint8_t tcid);
float SPI_GetTCColdReading(uint8_t tcid);

#endif /* MAX31855_H_ */
