#ifndef ONEWIRE_H_
#define ONEWIRE_H_

uint32_t OneWire_Init( void );
int OneWire_PerformTemperatureConversion(void);
float OneWire_GetTempSensorReading(void);
int OneWire_IsTCPresent(uint8_t tcid);
float OneWire_GetTCReading(uint8_t tcid);
float OneWire_GetTCColdReading(uint8_t tcid);

#endif /* ONEWIRE_H_ */
