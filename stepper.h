#ifndef __STEPPER_H__
#define __STEPPER_H__

//////////////////////////////////////////////////////////////////////////////////
// LCAM Stepper driver
// Written 2014 by Jimmy Alzén
//////////////////////////////////////////////////////////////////////////////////

// Globals:
extern int step_pos;
extern int step_target;

void stepInit();
void stepStep();
void stepRun();
void stepZero();

#endif //__STEPPER_H__
