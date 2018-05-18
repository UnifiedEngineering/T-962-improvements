
#include "LPC214x.h"
#include <stdint.h>
#include <stdio.h>
#include "adc.h"
#include "t962.h"
#include "onewire.h"
#include "max31855.h"
#include "nvstorage.h"

#include "sensor.h"
#include "config.h"

/*
 * Gain adjust, this may have to be calibrated per device if factory trimmer adjustments are off
 * Offset adjust, this will definitely have to be calibrated per device
 */
static struct { float offset; float gain; } adjust[2];
static float temperature[TC_NUM_ITEMS];
/* the smallest weight creates the lowest radiation output */
static float control_weight = 0.0f;

// values used in config.h
#define TC_NONE			0
#define TC_INTERNAL		1
#define TC_ONE_WIRE		2
#define TC_SPI_BRIDGE	3

#define LR_AVERAGE			0
#define MAX_TEMP_OVERRIDE	1
#define LR_WEIGHTED_AVERAGE	2

#ifndef CONTROL_TEMPERATURE
#define CONTROL_TEMPERATURE LR_AVERAGE
#endif

/* function pointers to select the correct functions for a sensor */
typedef float (*getTemperature)(uint8_t);
/* the onewire and spi functions deliver 1 if present, 0 if not */
typedef int (*isPresent)(uint8_t);

typedef struct {
	isPresent present;
	getTemperature T;
	getTemperature CJT;
} accessor_t;

#define UNAVAILABLE_TEMPERATURE		0.0f

float none_T(uint8_t channel) { UNUSED(channel); return UNAVAILABLE_TEMPERATURE; }
float none_CJT(uint8_t channel) { UNUSED(channel); return UNAVAILABLE_TEMPERATURE; }
int none_present(uint8_t channel) { UNUSED(channel); return 0; }

float internal_T(uint8_t channel)
{
	switch (channel) {
	case TC_LEFT:
		return ADC_Read(1) / 16.0f * adjust[0].gain + adjust[0].offset;
	case TC_RIGHT:
		return ADC_Read(2) / 16.0f * adjust[1].gain + adjust[1].offset;
	}
	return UNAVAILABLE_TEMPERATURE;
}

/* the internal interface is the DS18B20 if available, 25C otherwise */
float internal_CJT(uint8_t channel)
{
	UNUSED(channel);
	float cjt = OneWire_GetTempSensorReading();

	return cjt < 127.0f ? cjt : 25.0f;
}

int internal_present(uint8_t channel)
{
	switch (channel) {
	case TC_LEFT:
	case TC_RIGHT:
		return 1;
	case TC_COLD_JUNCTION:
		return OneWire_GetTempSensorReading() < 127.0f ? 1 : 0;
	}

	return 0;
}

/* access functions for ADC and buses */
static const accessor_t accessors[] = {
	[TC_NONE]		= { .present = none_present, .T = none_T, .CJT = none_CJT },
	[TC_INTERNAL]	= { .present = internal_present, .T = internal_T, .CJT = internal_CJT },
	[TC_ONE_WIRE]	= { .present = OneWire_IsTCPresent, .T = OneWire_GetTCReading, .CJT = OneWire_GetTCColdReading },
	[TC_SPI_BRIDGE] = { .present = SPI_IsTCPresent, .T = SPI_GetTCReading, .CJT = SPI_GetTCColdReading },
};

/*
 * first value is the interface, second value the channel
 * Note: not initialized values will be zero!
 */
static const uint8_t TC_if[][2] = {
	[TC_LEFT] = { TC_LEFT_IF },
	[TC_RIGHT] = { TC_RIGHT_IF },
	[TC_EXTRA1] = { TC_EXTRA1_IF },
	[TC_EXTRA2] = { TC_EXTRA2_IF },
	[TC_COLD_JUNCTION] = { COLD_JUNCTION_IF },
};

/* helper to access interface and channel */
static inline float get_T(TempSensor_t sensor)
{
	uint8_t _if = TC_if[sensor][0];
	uint8_t ch = TC_if[sensor][1];

	return accessors[_if].T(ch) + accessors[_if].CJT(ch);
}

/*
 * calculate the oven control temperature, depending on the configuration,
 * default is average of TC_LEFT and TC_RIGHT
 */
static float control_T(void)
{
#if CONTROL_TEMPERATURE == LR_AVERAGE
	return (temperature[TC_LEFT] + temperature[TC_RIGHT]) / 2;

#elif CONTROL_TEMPERATURE == MAX_TEMP_OVERRIDE
	float T_avglr = (temperature[TC_LEFT] + temperature[TC_RIGHT]) / 2;
	float T_avg = 0.0f;
	float T_max = 0.0f;
	int count = 0;

	// calculate max and average of present TCs
	for (unsigned i=0; i<4; i++)
		if (Sensor_IsValid(i)) {
			float T = get_T(i);

			count++;
			if (T > T_max)
				T_max = T;
			T_avg += T;
		}
	T_avg /= count;

	return T_max > T_avg + 5.0f ? T_avglr : T_avg;

#elif CONTROL_TEMPERATURE == LR_WEIGHTED_AVERAGE
	// this assumes the heavier weight is placed on the right TC!
	return temperature[TC_RIGHT] * control_weight + temperature[TC_LEFT] * (1.0f - control_weight);

#else
	#error "CONTROL_TEMPERATURE is not configured correctly"
#endif
}

/*
 * preset locals from EEPROM, which is initialized to 100 each
 * if EEPROM was not formatted.
 */
void Sensor_InitNV(void)
{
	adjust[0].gain = ((float) NV_GetConfig(TC_LEFT_GAIN)) * 0.01f;
	adjust[1].gain = ((float) NV_GetConfig(TC_RIGHT_GAIN)) * 0.01f;
	adjust[0].offset = ((float)(NV_GetConfig(TC_LEFT_OFFSET) - 100)) * 0.25f;
	adjust[1].offset = ((float)(NV_GetConfig(TC_RIGHT_OFFSET) - 100)) * 0.25f;
}

/*
 * set the control weight to a percent value 0..100, it will only be used if
 * CONTROL_TEMPERATURE is configured to be LR_WEIGHTED_AVERAGE
 */
void Sensor_SetWeight(int w)
{
	control_weight = w / 100.0f;
}

void Sensor_DoConversion(void)
{
	temperature[TC_LEFT] = get_T(TC_LEFT);
	temperature[TC_RIGHT] = get_T(TC_RIGHT);
	temperature[TC_EXTRA1] = get_T(TC_EXTRA1);
	temperature[TC_EXTRA2] = get_T(TC_EXTRA2);
	// this does not average as the original code did, but takes first channel info
	temperature[TC_COLD_JUNCTION] = accessors[COLD_JUNCTION_IF].CJT(0);
	// relies on above!
	temperature[TC_CONTROL] = control_T();
}

float Sensor_GetTemp(TempSensor_t sensor)
{
	// not safe for NUM_ITEMS, but who cares
	return temperature[sensor];
}

uint8_t Sensor_IsValid(TempSensor_t sensor)
{
	if (sensor == TC_CONTROL)		// control has no interface!
		return 1;
	const uint8_t *_if = TC_if[sensor];

	return accessors[_if[0]].present(_if[1]);
}
