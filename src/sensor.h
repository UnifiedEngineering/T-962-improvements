#ifndef SENSORS_H_
#define SENSORS_H_

typedef enum {
	TC_LEFT,
	TC_RIGHT,
	TC_EXTRA1,
	TC_EXTRA2,
	TC_COLD_JUNCTION,
	TC_CONTROL,
	TC_NUM_ITEMS
} TempSensor_t;


void Sensor_InitNV(void);
void Sensor_DoConversion(void);

float Sensor_GetTemp(TempSensor_t sensor);
uint8_t Sensor_IsValid(TempSensor_t sensor);
void Sensor_SetWeight(int w);
void Sensor_TweakWhileCooling(void);

#endif /* SENSORS_H_ */
