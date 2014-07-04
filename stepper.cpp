#include "Arduino.h"
#include "LCAM.h"

//////////////////////////////////////////////////////////////////////////////////
// LCAM Stepper driver
// Written 2014 by Jimmy Alzén
//////////////////////////////////////////////////////////////////////////////////

// Globals:
int step_pos = 0;
int step_target = 0;

void stepInit() {
  pinMode( S_STEP,   OUTPUT );
  pinMode( S_DIR,    OUTPUT );
  pinMode( S_M1,     OUTPUT );
  pinMode( S_M2,     OUTPUT );
  pinMode( S_ENABLE, OUTPUT );
  pinMode( S_LIMITMIN, INPUT );
  pinMode( S_LIMITMAX, INPUT );

  digitalWrite( S_STEP, LOW );
  digitalWrite( S_ENABLE, LOW );

  // Microstepping:
  //
  //     1/1 1/2 1/4 1/8
  // MS1  L   H   L   H
  // MS2  L   L   H   H

  digitalWrite( S_M1, LOW );   // HIGH
  digitalWrite( S_M2, HIGH );
  digitalWrite( S_DIR, LOW );
}

void stepStep() {
  delayMicroseconds(5);
  digitalWrite( S_STEP, HIGH );
  delayMicroseconds(10);
  digitalWrite( S_STEP, LOW );
  delayMicroseconds(10);
}

void stepRun() {
  if( !digitalRead( S_LIMITMIN ) ) {
    // Min limit trip
    if( step_target<step_pos ) step_target=step_pos;
  }
  if( !digitalRead( S_LIMITMAX ) ) {
    // Max limit trip
    if( step_target>step_pos ) step_target=step_pos;
  }
  if( step_target>step_pos ) {
    digitalWrite( S_DIR, HIGH );
    stepStep();
    step_pos++;
  } else if( step_target<step_pos ) {
    digitalWrite( S_DIR, LOW );
    stepStep();
    step_pos--;
  }
}

void stepZero() {
  // 1) If at min limit: run 5 mm in + dir
  if( !digitalRead( S_LIMITMIN ) ) {
    // Min limit trip
    step_target = 5.0/LCAM_XCONVF;
    while( step_pos != step_target ) {
      stepRun();
      delay(1);
    }
  }

  // 2) Run in - dir until at min limit
  step_target = step_pos-LCAM_XSIZEMM/LCAM_XCONVF;
  while( step_pos!=step_target ) {
    stepRun();
    delay(1);
    if( !digitalRead( S_LIMITMIN ) ) {
      // 3) Set zero point
      step_pos=0;
      step_target=0;
      return;
    }
  }
}
