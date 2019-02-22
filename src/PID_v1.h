#ifndef PID_H
#define PID_H

typedef float FloatType;
//typedef double floatType;
#include <stdbool.h>

//Constants used in some of the functions below
typedef enum
{
  PID_Mode_Automatic = 1,
  PID_Mode_Manual    = 0
} PidModeType;

typedef enum
{
  PID_Direction_Direct  = 0,
  PID_Direction_Reverse = 1
} PidDirectionType;

typedef struct {
  FloatType dispKp; // * we'll hold on to the tuning parameters in user-entered
  FloatType dispKi; //   format for display purposes
  FloatType dispKd; //

  FloatType kp; // * (P)roportional Tuning Parameter
  FloatType ki; // * (I)ntegral Tuning Parameter
  FloatType kd; // * (D)erivative Tuning Parameter

  PidDirectionType controllerDirection;

  FloatType myInput; // * Pointers to the Input, Output, and Setpoint variables
  FloatType myOutput; //   This creates a hard link between the variables and the
  FloatType mySetpoint; //   PID, freeing the user from having to constantly tell us
                     //   what these values are.  with pointers we'll just know.

//  unsigned long lastTime;
  FloatType ITerm, lastInput;

  unsigned long SampleTime;
  FloatType outMin, outMax;
  bool inAuto;
} PidType;

//commonly used functions **************************************************************************

//  constructor.  links the PID to the Input, Output, and
//  Setpoint.  Initial tuning parameters are also set here
void PID_init(PidType* pid,
    FloatType kp,
    FloatType ki,
    FloatType kd,
    PidDirectionType controllerDirection);

// sets PID to either Manual (0) or Auto (non-0)
void PID_SetMode(PidType* pid, PidModeType mode);

// performs the PID calculation.  it should be
// called every time loop() cycles. ON/OFF and
// calculation frequency can be set using SetMode
// SetSampleTime respectively
bool PID_Compute(PidType* pid);

// clamps the output to a specific range. 0-255 by default, but
// it's likely the user will want to change this depending on
// the application
void PID_SetOutputLimits(PidType* pid, FloatType min, FloatType max);

//available but not commonly used functions ********************************************************

// While most users will set the tunings once in the
// constructor, this function gives the user the option
// of changing tunings during runtime for Adaptive control
void PID_SetTunings(PidType* pid, FloatType kp, FloatType ki, FloatType kd);

// Sets the Direction, or "Action" of the controller. DIRECT
// means the output will increase when error is positive. REVERSE
// means the opposite.  it's very unlikely that this will be needed
// once it is set in the constructor.
void PID_SetControllerDirection(PidType* pid, PidDirectionType Direction);

// sets the frequency, in Milliseconds, with which
// the PID calculation is performed.  default is 100
void PID_SetSampleTime(PidType* pid, int newSampleTime);

//Display functions ****************************************************************
// These functions query the pid for interal values.
//  they were created mainly for the pid front-end,
// where it's important to know what is actually
//  inside the PID.
FloatType PID_GetKp(PidType* pid);
FloatType PID_GetKi(PidType* pid);
FloatType PID_GetKd(PidType* pid);
PidModeType PID_GetMode(PidType* pid);
PidDirectionType PID_GetDirection(PidType* pid);

//void PID_Initialize(PidType* pid);
#endif
