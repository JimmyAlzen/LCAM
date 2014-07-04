#include <SD.h>
#include "sdcard.h"

//////////////////////////////////////////////////////////////////////////////////
// All SD card disk operations
// Written 2014 by Jimmy Alzén
//////////////////////////////////////////////////////////////////////////////////

// Persistent settings, loaded on startup
SETTINGS gSettings;
DARKFRAME2 gDarkframe;
GAINCAL2 gGaincal;

File gFile;

boolean sdReadSettings() {
  gSettings.ver=1;
  gSettings.image_number=0;
  if( gFile = SD.open( "lcam.cfg", FILE_READ ) ) {
    unsigned char *rptr = (uint8_t*)&gSettings;
    int readbytes=0;
    while( gFile.available()&&(readbytes<sizeof(SETTINGS)) ) {
      *rptr++ = gFile.read();
      readbytes++;
    }
    gFile.close();
    return true;
  }
  return false;
}

void sdWriteSettings() {
  if( gFile = SD.open( "lcam.cfg", FILE_WRITE ) ) {
    gFile.seek(0);
    unsigned char *rptr = (uint8_t*)&gSettings;
    gFile.write( rptr, sizeof(SETTINGS) );
    gFile.close();
  }
}

void sdZeroSettings() {
  gSettings.ver=1;
  gSettings.image_number=0;
}

void sdCreateImage() {
  gSettings.image_number;
  char fname[32];
  sprintf( fname, "DSCJ%04d.RAW", gSettings.image_number );
  gFile = SD.open( fname, FILE_WRITE );
}

void sdWriteImage( int *p_ccd_line, int _oversample ) {
  unsigned short img_line[1536];
  // left-justify data
  for( int i=0;i<1536;i++ ) {
    int s = (p_ccd_line[i]*16)/_oversample;
    if( s>65535 ) s = 65535;
    if( s<0 ) s = 0;
    img_line[i] = (unsigned short)( s );
  }
  // [!] byte order is LSB first in stream
  gFile.write( (const uint8_t*)img_line, 1536*2 );
}

void sdCloseImage() {
  gFile.close();
}

void sdWriteDarkframe() {
  if( gFile = SD.open( "DARKFRM.RAW", FILE_WRITE ) ) {
    gFile.seek(0);
    unsigned char *rptr = (uint8_t*)&gDarkframe.ccd_line;
    gFile.write( rptr, sizeof(DARKFRAME2) );
    gFile.close();
  }
}

boolean sdReadDarkframe() {
  gDarkframe.ver=1;
  for( int s=0;s<7;s++ ) {
    for( int i=0;i<1536;i++ ) {
      gDarkframe.ccd_line[s][i] = 0;
    }
  }
  if( gFile = SD.open( "DARKFRM.RAW", FILE_READ ) ) {
    unsigned char *rptr = (uint8_t*)&gDarkframe.ccd_line;
    int readbytes=0;
    while( gFile.available()&&(readbytes<sizeof(DARKFRAME2)) ) {
      *rptr++ = gFile.read();
      readbytes++;
    }
    gFile.close();
    return true;
  }
  return false;
}

void sdWriteGaincal() {
  if( gFile = SD.open( "GAINCAL.RAW", FILE_WRITE ) ) {
    gFile.seek(0);
    gGaincal.ver = 2;
    unsigned char *rptr = (uint8_t*)&gGaincal.gain;
    gFile.write( rptr, sizeof(GAINCAL2) );
    gFile.close();
  }
}

boolean sdReadGaincal() {
  gGaincal.ver=2;
  for( int s=0;s<7;s++ ) {
    for( int i=0;i<1536;i++ ) {
      gGaincal.gain[s][i] = LCAM_GAIN_BASIS;
    }
  }
  if( gFile = SD.open( "GAINCAL.RAW", FILE_READ ) ) {
    unsigned char *rptr = (uint8_t*)&gGaincal.gain;
    int readbytes=0;
    while( gFile.available()&&(readbytes<sizeof(GAINCAL2)) ) {
      *rptr++ = gFile.read();
      readbytes++;
    }
    gFile.close();
    return true;
  }
  return false;
}

