#ifndef __LCAM_H__
#define __LCAM_H__

//////////////////////////////////////////////////////////////////////////////////
// LCAM Hardware definitions
// Written 2014 by Jimmy Alzén
//////////////////////////////////////////////////////////////////////////////////

//  LCAM Pin Assignments:
//
//  SPI Pins --------------------
//
//  SPI1 IN    MISO
//  SPI3 OUT   SCK
//  SPI4 OUT   MOSI
//
//  Digital Pins ----------------
//
//  22   OUT   TFT CS
//  24   OUT   SD CS
//  26   OUT   TFT Backlight
//  28   OUT   TFT Reset
//  30   OUT   TFT D/C

//  32   OUT   Stepper STEP
//  34   OUT   Stepper DIR
//  36   OUT   Stepper M1
//  38   OUT   Stepper M2
//  40   OUT   Stepper ENABLE
//
//  42   OUT   CCD SI
//
//  PWM Pins --------------------
//
//  6    OUT   CCD CLK
//
//  Analog Pins -----------------
//
//  A0   IN    CCD A
//
//  User Interface Pins ---------
//
//  B1  IN    44  OK/ENTER
//  B2  IN    46  RIGHT
//  B3  IN    48  LEFT
//  B4  IN    50  DOWN
//  B5  IN    51  UP
//  B6  IN    52  CANCEL/BACK

#define SD_CS      24
#define TFT_CS     22
#define TFT_BL     26
#define TFT_RESET  28
#define TFT_DC     30

#define S_STEP     32
#define S_DIR      34
#define S_M1       36
#define S_M2       38
#define S_ENABLE   40  // Active low
#define S_LIMITMIN 25  // Active low : low = trip
#define S_LIMITMAX 23  // Active low : low = trip

#define CCD_SI     42
#define CCD_CLK    6    // PWM channel
#define CCD_A      0    // A/D Channel

#define  B1        44
#define  B2        46
#define  B3        48
#define  B4        50
#define  B5        51
#define  B6        52

// Camera physical parameters
// number of steps = 8490 steps (1/8 microsteps)
//                 = 4245 steps (1/4 microsteps)
// traveled distance = 138.0 mm
// conversion factor = 32.51 um/step
//
// Skip 50 steps as safety margin

#define LCAM_XSIZE   (4195)        // steps
#define LCAM_XSIZEMM (138.0)       // mm
#define LCAM_XCONVF  (0.032508834) // mm/step

#endif //__LCAM_H__
