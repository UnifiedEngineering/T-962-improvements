#ifndef SENSORS_H_
#define SENSORS_H_

typedef enum eTempSensor {
	TC_COLD_JUNCTION=0,
	TC_AVERAGE,
	TC_LEFT,
	TC_RIGHT,
	TC_EXTRA1,
	TC_EXTRA2,
	TC_NUM_ITEMS
} TempSensor_t;


void Sensor_ValidateNV(void);
void Sensor_DoConversion(void);

uint8_t Sensor_ColdjunctionPresent(void);

float Sensor_GetTemp(TempSensor_t sensor);
uint8_t Sensor_IsValid(TempSensor_t sensor);

void Sensor_ListAll(void);

#endif /* SENSORS_H_ */
