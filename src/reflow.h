#ifndef REFLOW_H_
#define REFLOW_H_

typedef enum {
	REFLOW_STANDBY,
	REFLOW_BAKE_PREHEAT,
	REFLOW_BAKE,
	REFLOW_REFLOW,
	REFLOW_COOLING
} ReflowState_t;

typedef struct {
	uint8_t heater;
	uint8_t fan;
	float setpoint;
	float temperature;
	uint32_t time_done;
	uint32_t time_to_go;
} ReflowInformation_t;

#define SETPOINT_MIN 30
#define SETPOINT_MAX 260

// 36 hours max timer
#define BAKE_TIMER_MAX (60 * 60 * 36)

void Reflow_Init(void);
void Reflow_SetLogLevel(int);
const ReflowInformation_t *Reflow_Information(void);
int Reflow_ActivateBake(int, int);
int Reflow_ActivateReflow(void);
void Reflow_abort(void);

#endif /* REFLOW_H_ */
