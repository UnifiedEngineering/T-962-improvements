#ifndef REFLOW_H_
#define REFLOW_H_

typedef enum eReflowMode {
	REFLOW_INITIAL=0,
	REFLOW_STANDBY,
	REFLOW_BAKE,
	REFLOW_REFLOW,
	REFLOW_STANDBYFAN
} ReflowMode_t;

void Reflow_Init(void);
void Reflow_SetMode(ReflowMode_t themode);
void Reflow_SetSetpoint(uint16_t thesetpoint);
int16_t Reflow_GetActualTemp(void);
uint8_t Reflow_IsDone(void);

uint16_t Reflow_GetSetpoint(void);
void Reflow_SetBakeTimer(int seconds);
int Reflow_GetTimeLeft(void);
int32_t Reflow_Run(uint32_t thetime, float meastemp, uint8_t* pheat, uint8_t* pfan, int32_t manualsetpoint);
void Reflow_ToggleStandbyLogging(void);

#endif /* REFLOW_H_ */
