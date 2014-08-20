#include <SD.h>
#include "sdcard.h"
#include <SPI.h>

//////////////////////////////////////////////////////////////////////////////////
// All SD card disk operations
// Written 2014 by Jimmy Alzén
//////////////////////////////////////////////////////////////////////////////////

// Persistent settings, loaded on startup
SETTINGS gSettings;
DARKFRAME2 gDarkframe;
GAINCAL2 gGaincal;
NETWORK gNetwork;

File gFile;

int fgets( File *pfile, char *dst, int dstsize ) {
  int n=0;
  char c; 
  while( n<dstsize && pfile->available() && (c=pfile->read())!='\n') {
    if( (c!='\r') ) {
      dst[n++] = c;
    }
  }
  dst[n] = 0;
  return n;
}

boolean sdReadSettings() {
  noInterrupts();
  SPI.setDataMode(SPI_MODE0);

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
    SPI.setDataMode(SPI_MODE1);
    interrupts();
    return true;
  }
  SPI.setDataMode(SPI_MODE1);
  interrupts();
  return false;
}


boolean sdReadNetwork() {
  noInterrupts();
  SPI.setDataMode(SPI_MODE0);
  
  char line[33];
  line[0] = 0;
  if( gFile = SD.open( "network.cfg", FILE_READ ) ) {
    int n=0;
    n = fgets( &gFile, line, sizeof(line)-1 );
    if( n>0 ) {
      line[n] = 0;
      strcpy( gNetwork.ssid, line );
    }
    n = fgets( &gFile, line, sizeof(line) );
    if( n>0 ) {
      line[n] = 0;
      strcpy( gNetwork.password, line );
    }
    gFile.close();
  } else {
    SPI.setDataMode(SPI_MODE1);
    interrupts();
    return false;
  }
  SPI.setDataMode(SPI_MODE1);
  interrupts();
  return true;
}


void sdWriteSettings() {
  noInterrupts();
  SPI.setDataMode(SPI_MODE0);
  
  if( gFile = SD.open( "lcam.cfg", FILE_WRITE ) ) {
    gFile.seek(0);
    unsigned char *rptr = (uint8_t*)&gSettings;
    gFile.write( rptr, sizeof(SETTINGS) );
    gFile.close();
  }
  SPI.setDataMode(SPI_MODE1);
  interrupts();  
}

void sdZeroSettings() {
  gSettings.ver=1;
  gSettings.image_number=0;
}

void sdCreateImage() {
  noInterrupts();
  SPI.setDataMode(SPI_MODE0);

  gSettings.image_number;
  char fname[32];
  sprintf( fname, "DSCJ%04d.RAW", gSettings.image_number );
  gFile = SD.open( fname, FILE_WRITE );
  SPI.setDataMode(SPI_MODE1);
  interrupts();  
}

void sdWriteImage( int *p_ccd_line, int _oversample ) {
  unsigned short img_line[1536];
  
  noInterrupts();
  SPI.setDataMode(SPI_MODE0);

  // left-justify data
  for( int i=0;i<1536;i++ ) {
    int s = (p_ccd_line[i]*16)/_oversample;
    if( s>65535 ) s = 65535;
    if( s<0 ) s = 0;
    img_line[i] = (unsigned short)( s );
  }
  // [!] byte order is LSB first in stream
  gFile.write( (const uint8_t*)img_line, 1536*2 );
  SPI.setDataMode(SPI_MODE1);
  interrupts();  
}

void sdCloseImage() {
  noInterrupts();
  SPI.setDataMode(SPI_MODE0);
  
  gFile.close();
  SPI.setDataMode(SPI_MODE1);
  interrupts();  
}

void sdWriteDarkframe() {
  noInterrupts();
  SPI.setDataMode(SPI_MODE0);

  if( gFile = SD.open( "DARKFRM.RAW", FILE_WRITE ) ) {
    gFile.seek(0);
    unsigned char *rptr = (uint8_t*)&gDarkframe.ccd_line;
    gFile.write( rptr, sizeof(DARKFRAME2) );
    gFile.close();
  }
  SPI.setDataMode(SPI_MODE1);
  interrupts();  
}

boolean sdReadDarkframe() {
  noInterrupts();
  SPI.setDataMode(SPI_MODE0);

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
    SPI.setDataMode(SPI_MODE1);
    interrupts();    
    return true;
  }
  SPI.setDataMode(SPI_MODE1);
  interrupts();  
  return false;
}

void sdWriteGaincal() {
  noInterrupts();
  SPI.setDataMode(SPI_MODE0);

  if( gFile = SD.open( "GAINCAL.RAW", FILE_WRITE ) ) {
    gFile.seek(0);
    gGaincal.ver = 2;
    unsigned char *rptr = (uint8_t*)&gGaincal.gain;
    gFile.write( rptr, sizeof(GAINCAL2) );
    gFile.close();
  }
  SPI.setDataMode(SPI_MODE1);
  interrupts();  
}

boolean sdReadGaincal() {
  noInterrupts();
  SPI.setDataMode(SPI_MODE0);

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
    SPI.setDataMode(SPI_MODE1);
    interrupts();    
    return true;
  }
  SPI.setDataMode(SPI_MODE1);
  interrupts();
  return false;
}

