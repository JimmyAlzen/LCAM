#ifndef __SDCARD_H__
#define __SDCARD_H__

//////////////////////////////////////////////////////////////////////////////////
// All SD card disk operations
// Written 2014 by Jimmy Alzén
//////////////////////////////////////////////////////////////////////////////////

typedef struct {
  int ver;
  int image_number;
} SETTINGS;

typedef struct DARKFRAME {
  int ver;
  float ccd_line[1536];
} DARKFRAME;

typedef struct DARKFRAME2 {
  int ver;
  short ccd_line[7][1536];
} DARKFRAME2;

typedef struct GAINCAL {
  int ver;
  float gain[1536];
} GAINCAL;

typedef struct GAINCAL2 {
  int ver;
  short gain[7][1536];
} GAINCAL2;

#define LCAM_GAIN_BASIS  16384
#define LCAM_GAIN_BITS   14

extern SETTINGS gSettings;
extern DARKFRAME2 gDarkframe;
extern GAINCAL2 gGaincal;
extern File gFile;

boolean sdReadSettings();
void sdWriteSettings();
void sdZeroSettings();
void sdCreateImage();
void sdWriteImage( int *p_ccd_line, int _oversample );
void sdCloseImage();
void sdWriteDarkframe();
boolean sdReadDarkframe();
void sdWriteGaincal();
boolean sdReadGaincal();

#endif //__SDCARD_H__


