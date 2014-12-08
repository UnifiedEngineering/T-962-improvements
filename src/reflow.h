#ifndef REFLOW_H_
#define REFLOW_H_

typedef enum eReflowMode {
	REFLOW_STANDBY=0,
	REFLOW_BAKE,
	REFLOW_REFLOW
} ReflowMode_t;

typedef enum eTempSensor {
	TC_COLD_JUNCTION=0,
	TC_AVERAGE,
	TC_LEFT,
	TC_RIGHT,
	TC_EXTRA1,
	TC_EXTRA2,
	TC_NUM_ITEMS
} TempSensor_t;

void Reflow_Init(void);
void Reflow_SetMode(ReflowMode_t themode);
void Reflow_SetSetpoint(uint16_t thesetpoint);
int16_t Reflow_GetActualTemp(void);
float Reflow_GetTempSensor(TempSensor_t sensor);
uint8_t Reflow_IsTempSensorValid(TempSensor_t sensor);
uint8_t Reflow_IsDone(void);
void Reflow_PlotProfile(int highlight);
int Reflow_GetProfileIdx(void);
int Reflow_SelectProfileIdx(int idx);
int Reflow_SelectEEProfileIdx(int idx);
int Reflow_SaveEEProfile(void);
const char* Reflow_GetProfileName(void);
uint16_t Reflow_GetSetpointAtIdx(uint8_t idx);
void Reflow_SetSetpointAtIdx(uint8_t idx, uint16_t value);
uint16_t Reflow_GetSetpoint(void);
int32_t Reflow_Run(uint32_t thetime, float meastemp, uint8_t* pheat, uint8_t* pfan, int32_t manualsetpoint);

#endif /* REFLOW_H_ */
