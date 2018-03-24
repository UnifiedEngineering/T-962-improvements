
#include "LPC214x.h"
#include <stdint.h>
#include <stdio.h>
#include "adc.h"
#include "t962.h"
#include "onewire.h"
#include "max31855.h"
#include "nvstorage.h"

#include "sensor.h"

/*
* Normally the control input is the average of the first two TCs.
* By defining this any TC that has a readout 5C (or more) higher
* than the TC0 and TC1 average will be used as control input instead.
* Use if you have very sensitive components. Note that this will also
* kick in if the two sides of the oven has different readouts, as the
* code treats all four TCs the same way.
*/
//#define MAXTEMPOVERRIDE

// Operational Mode
static OperationMode_t opMode = AMBIENT;
static uint8_t opModeTempThresh = 5;
// Gain adjust, this may have to be calibrated per device if factory trimmer adjustments are off
static float adcgainadj[2];
 // Offset adjust, this will definitely have to be calibrated per device
static float adcoffsetadj[2];

static float temperature[4] = { 0.0f, 0.0f, 0.0f, 0.0f };
static uint8_t tempvalid = 0;
static uint8_t cjsensorpresent = 0;

// The feedback temperature
static float avgtemp;
static float coldjunction;

OperationMode_t Sensor_getOpMode() {
	return opMode;
}

void Sensor_printOpMode() {
	const char* modes[] = { "AMBIENT", "MAXTEMPOVERRIDE", "SPLIT" };
	//printf("\nCurrent Operational Mode: %s\n", modes[(uint8_t)Sensor_getOpMode()]);
	printf("%s", modes[(uint8_t)Sensor_getOpMode()]);
}

void Sensor_setOpMode(OperationMode_t newmode) {
	opMode = newmode;
	NV_SetConfig(OP_MODE, (uint8_t)newmode);
}

uint8_t Sensor_getOpModeThreshold()
{
	return opModeTempThresh;
}

void Sensor_setOpModeThreshold(uint8_t threshold)
{
	opModeTempThresh = threshold;
	NV_SetConfig(MODE_THRESH, threshold);
}

void Sensor_ValidateNV(void) {
	int temp;

	temp = NV_GetConfig(TC_LEFT_GAIN);
	if (temp == 255) {
		temp = 100;
		NV_SetConfig(TC_LEFT_GAIN, temp); // Default unity gain
	}
	adcgainadj[0] = ((float)temp) * 0.01f;

	temp = NV_GetConfig(TC_RIGHT_GAIN);
	if (temp == 255) {
		temp = 100;
		NV_SetConfig(TC_RIGHT_GAIN, temp); // Default unity gain
	}
	adcgainadj[1] = ((float)temp) * 0.01f;

	temp = NV_GetConfig(TC_LEFT_OFFSET);
	if (temp == 255) {
		temp = 100;
		NV_SetConfig(TC_LEFT_OFFSET, temp); // Default +/-0 offset
	}
	adcoffsetadj[0] = ((float)(temp - 100)) * 0.25f;

	temp = NV_GetConfig(TC_RIGHT_OFFSET);
	if (temp == 255) {
		temp = 100;
		NV_SetConfig(TC_RIGHT_OFFSET, temp); // Default +/-0 offset
	}
	adcoffsetadj[1] = ((float)(temp - 100)) * 0.25f;

	temp = NV_GetConfig(OP_MODE);
	opMode = (OperationMode_t)temp;

	temp = NV_GetConfig(MODE_THRESH);
	opModeTempThresh = temp;
}


void Sensor_DoConversion(void) {
	uint16_t temp[2];
	/*
	* These are the temperature readings we get from the thermocouple interfaces
	* Right now it is assumed that if they are indeed present the first two
	* channels will be used as feedback
	*/
	float tctemp[4], tccj[4];
	uint8_t tcpresent[4];
	tempvalid = 0; // Assume no valid readings;
	for (int i = 0; i < 4; i++) { // Get 4 TC channels
		tcpresent[i] = OneWire_IsTCPresent(i);
		if (tcpresent[i]) {
			tctemp[i] = OneWire_GetTCReading(i);
			tccj[i] = OneWire_GetTCColdReading(i);
			if (i > 1) {
				temperature[i] = tctemp[i];
				tempvalid |= (1 << i);
			}
		}
		else {
			tcpresent[i] = SPI_IsTCPresent(i);
			if (tcpresent[i]) {
				tctemp[i] = SPI_GetTCReading(i);
				tccj[i] = SPI_GetTCColdReading(i);
				if (i > 1) {
					temperature[i] = tctemp[i];
					tempvalid |= (1 << i);
				}
			}
		}
	}

	// Assume no CJ sensor
	cjsensorpresent = 0;
	if (tcpresent[0] && tcpresent[1]) {
		avgtemp = (tctemp[0] + tctemp[1]) / 2.0f;
		temperature[0] = tctemp[0];
		temperature[1] = tctemp[1];
		tempvalid |= 0x03;
		coldjunction = (tccj[0] + tccj[1]) / 2.0f;
		cjsensorpresent = 1;
	}
	else if (tcpresent[2] && tcpresent[3]) {
		avgtemp = (tctemp[2] + tctemp[3]) / 2.0f;
		temperature[0] = tctemp[2];
		temperature[1] = tctemp[3];
		tempvalid |= 0x03;
		tempvalid &= ~0x0C;
		coldjunction = (tccj[2] + tccj[3]) / 2.0f;
		cjsensorpresent = 1;
	}
	else {
		// If the external TC interface is not present we fall back to the
		// built-in ADC, with or without compensation
		coldjunction = OneWire_GetTempSensorReading();
		if (coldjunction < 127.0f) {
			cjsensorpresent = 1;
		}
		else {
			coldjunction = 25.0f; // Assume 25C ambient if not found
		}
		temp[0] = ADC_Read(1);
		temp[1] = ADC_Read(2);

		// ADC oversamples to supply 4 additional bits of resolution
		temperature[0] = ((float)temp[0]) / 16.0f;
		temperature[1] = ((float)temp[1]) / 16.0f;

		// Gain adjust
		temperature[0] *= adcgainadj[0];
		temperature[1] *= adcgainadj[1];

		// Offset adjust
		temperature[0] += coldjunction + adcoffsetadj[0];
		temperature[1] += coldjunction + adcoffsetadj[1];

		tempvalid |= 0x03;

		avgtemp = (temperature[0] + temperature[1]) / 2.0f;
	}

	// if the mode is not AMBIENT, we override avgtemp based on mode
	switch (opMode) {
		case MAXTEMPOVERRIDE: {
			// If one of the temperature sensors reports higher than 5C above
			// the average, use that as control input
			float newtemp = avgtemp;
			for (int i = 0; i < 4; i++) {
				if (tcpresent[i] && temperature[i] > (avgtemp + (float)opModeTempThresh) && temperature[i] > newtemp) {
					newtemp = temperature[i];
				}
			}
			if (avgtemp != newtemp) {
				avgtemp = newtemp;
			}
			break;
		}
		case SPLIT: {
			//Override avgtemp to board temp if > splitTempThresh
			if (avgtemp > opModeTempThresh) {
				if (tcpresent[2] && tcpresent[3]) {
					avgtemp = (tctemp[2] + tctemp[3]) / 2.0f;
				}
				else if (tcpresent[2]) {
					avgtemp = tctemp[2];
				}
				else if (tcpresent[3]) {
					avgtemp = tctemp[3];
				}
			}
			break;
		}
		case AMBIENT: {
			//default
		}
	}
}

uint8_t Sensor_ColdjunctionPresent(void) {
	return cjsensorpresent;
}


float Sensor_GetTemp(TempSensor_t sensor) {
	if (sensor == TC_COLD_JUNCTION) {
		return coldjunction;
	} else if(sensor == TC_AVERAGE) {
		return avgtemp;
	} else if(sensor < TC_NUM_ITEMS) {
		return temperature[sensor - TC_LEFT];
	} else {
		return 0.0f;
	}
}

uint8_t Sensor_IsValid(TempSensor_t sensor) {
	if (sensor == TC_COLD_JUNCTION) {
		return cjsensorpresent;
	} else if(sensor == TC_AVERAGE) {
		return 1;
	} else if(sensor >= TC_NUM_ITEMS) {
		return 0;
	}
	return (tempvalid & (1 << (sensor - TC_LEFT))) ? 1 : 0;
}

void Sensor_ListAll(void) {
	int count = 5;
	char* names[] = {"Left", "Right", "Extra 1", "Extra 2", "Cold junction"};
	TempSensor_t sensors[] = {TC_LEFT, TC_RIGHT, TC_EXTRA1, TC_EXTRA2, TC_COLD_JUNCTION};
	char* format = "\n%13s: %4.1fdegC";

	for (int i = 0; i < count; i++) {
		if (Sensor_IsValid(sensors[i])) {
			printf(format, names[i], Sensor_GetTemp(sensors[i]));
		}
	}
	if (!Sensor_IsValid(TC_COLD_JUNCTION)) {
		printf("\nNo cold-junction sensor on PCB");
	}
}
