#include <SPI.h>
#include <Adafruit_GFX.h>
#include <Adafruit_ILI9340.h>
#include <SD.h>
#include "LCAM.h"

 /////////////////////////////////////
///////////////////////////////////////
//                                   //
//  LCAM - Large Format Scan Camera  //
//  Written 2014   by   Jimmy Alzén  //
//                                   //
//  v2                               //
//                                   //
///////////////////////////////////////
 /////////////////////////////////////

// TODO List:
//
// [ ] Gain cal. doesnt work correctly
//   [ ] Capture gain calibration should capture gain cal. for current samplerate only and store in larger struct
// [ ] Capture astronomy image
// [ ] Place images in \DCIM to avoid limitations of FAT16 root directory
// [ ] 

// Improvements List:
// 
// [ ] Quick commands for driving sensor to middle and end positions
// [ ] Settings screen for adv. settings
// [ ] 


#if defined(__SAM3X8E__)
    #undef __FlashStringHelper::F(string_literal)
    #define F(string_literal) string_literal
#endif

// TFT Library Pin Definitions
//
#define _sclk 76
#define _miso 74
#define _mosi 75
#define _cs 22
#define _dc 30
#define _rst 28

// Using software SPI is really not suggested, its incredibly slow
//Adafruit_ILI9340 tft = Adafruit_ILI9340(_cs, _dc, _mosi, _sclk, _rst, _miso);
// Use hardware SPI
Adafruit_ILI9340 tft = Adafruit_ILI9340(_cs, _dc, _rst);

#include "sdcard.h"
#include "stepper.h"

#define PROCESSIMAGE    // Define to enable in-camera dark frame subtraction and gain calibration

// camera settings
int samplerate=320000; // Hz
int samplerate_idx=6;
int oversample=1;      // times
int coldelay=0;        // milliseconds
int scantime=167;      // seconds (only for display)
int inttime=43;        // milliseconds (only for display)

// camera state
int ccd_line[1536];
int camxpos=0;

int freeRam () 
{
  extern int __heap_start, *__brkval; 
  int v; 
  return (int) &v - (__brkval == 0 ? (int) &__heap_start : (int) __brkval); 
}

void setup() {
  boolean success=true;
  // Initialize TFT
    
  tft.begin();
  tft.setRotation(1);
  tft.fillScreen(ILI9340_BLACK);
  tft.setCursor(0, 0);
  tft.setTextColor(ILI9340_YELLOW); 
  tft.setTextSize(1);
  tft.println("LCAM v0.1 booting");
  
  // Initialize SD card
  tft.println("Initializing SD card...");
  if( !SD.begin( SD_CS ) ) {
    tft.println("SD init failed");
    success = false;
  } else {
    tft.println("SD init OK.");
  }  
  
  // Initialize Stepper
  stepInit();
  stepZero();
  tft.println("Stepper init OK." );
  
  // Initialize CCD
  ccdInit();
  tft.println("CCD init OK." );

  // Read settings
  tft.print("Reading settings..." );
  if( sdReadSettings() ) {
    tft.println("OK");
  } else {
    success = false;    
    tft.println("Not found");
  }
  
  tft.print("Reading dark frame..." );
  if( sdReadDarkframe() ) {
    tft.println("OK");
  } else {
    success = false;
    tft.println("Not found");
  }
    
  tft.print("Reading gain cal..." );
  if( sdReadGaincal() ) {
    tft.println("OK");
  } else {
    success = false;
    tft.println("Not found");
  }
  
  // Initialize UI
  uiInit();
  tft.println("UI init OK." );
  
  tft.println("Available RAM: " );
  tft.print( freeRam() );
  tft.println( " bytes" );
  
  // Wait for key press if showing error message
 // if( !success ) {
    tft.println("Press any key to continue ...");
    while( !scanKeys() )delay( 25 );
 // }
  
  uiCreateSetup();
  uiDrawPos();
}

//////////////////////////////////////////////////////////////////////////////////
//
// CCD
//
//////////////////////////////////////////////////////////////////////////////////

void ccdInit() {
  pinMode( CCD_SI,    OUTPUT );
  pinMode( CCD_CLK,   OUTPUT );
  
  digitalWrite( CCD_SI, LOW );
  digitalWrite( CCD_CLK, LOW );
  // Setup A/D
  analogReadResolution(12);
  // 250kHz A/D speed
  REG_ADC_MR = (REG_ADC_MR & 0xFFF0FFFF) | 0x00020000;
  for( int i=0;i<1536;i++ ) {
    ccd_line[i]=0;
  }  
}

// Fast I/O definitions
#define SI_0     PIOA->PIO_CODR=1<<19
#define SI_1     PIOA->PIO_SODR=1<<19
#define CLK_0    PIOC->PIO_CODR=1<<24
#define CLK_1    PIOC->PIO_SODR=1<<24

// delayMicroseconds(10) gives 72  kHz sample rate  Tc = 1.388888889e-5  Tw=1e-5  Td=3.88e-6
// delayMicroseconds(20) gives 42.2kHz sample rate  Tc = 2.369668246e-5  Tw=2e-5  Td=3.69e-6
// delayMicroseconds(30) gives 29.6kHz sample rate  Tc = 3.378378378e-5  Tw=3e-5  Td=3.78e-6
// delayMicroseconds(50) gives 18.7kHz sample rate  Tc = 5.347593583e-5  Tw=5e-5  Td=3.47e-6
// Avg of Td = Tda = 3.705
// Tactual = Tda + x
// Factual = 1/( Tda+x )
// Tda+x = 1/Factual
// x = 1/Factual-Tda

int ccdDelayFromFrequency( int smprate ) {
  float delay_s = (1.0/smprate)-3.705e-6;
  return (int)(1000000*delay_s);
}

#define CCD_COPY 0   // Copy image column to ccd_line
#define CCD_ADD  1   // Add image column to ccd_line
void ccdAcquire( int mode ) {
  int dl = ccdDelayFromFrequency( samplerate );
  int i;
  CLK_0;
  delayMicroseconds( dl );
  SI_1;
  delayMicroseconds( dl );
  CLK_1;
  delayMicroseconds( dl );
  SI_0;
  int frameidx=0;
  for( i=0;i<1536;i++ ) { // 1536+18
    CLK_0;
    //if( i>=18 && (frameidx<1536-1) ) {
      if( mode==0 ) {
        ccd_line[frameidx++] = analogRead(A0);
      } else {
        ccd_line[frameidx++] += analogRead(A0);
      }
    //} else {
    //  delayMicroseconds( dl );
    //}
    CLK_1;
    delayMicroseconds( dl );
  }
  CLK_0;
  delayMicroseconds( dl );  
  CLK_1;
  delayMicroseconds( dl );  
}

// Subtract dark frame
// Use gain calibration
void ccdProcessColumn() {
#ifdef PROCESSIMAGE  
  for( int i=0;i<1536;i++ ) {
    int s = ccd_line[i] - gDarkframe.ccd_line[samplerate_idx][i];
    if( s<0 ) s=0;
      s = (s*gGaincal.gain[samplerate_idx][i])>>LCAM_GAIN_BITS;
    if( s>65535 ) s=65535;
    ccd_line[i] = s;
  }
#endif  
}


//////////////////////////////////////////////////////////////////////////////////
//
// User Interface
//
//////////////////////////////////////////////////////////////////////////////////

// User interface modes:
//
// SETUP
//   Edit samplerate
//   Edit oversample
//   Edit column delay
//   Acquire image
//   
// ACQUIRE
//   Cancel acquire
//
// MENU
//   Capture dark frame
//   Capture gain cal.
//   Capture astronomy image
//   Exit menu
//

#define SETUP    1
#define ACQUIRE  2
#define MENU     3
int ui_mode=SETUP;

// ui state
int param=0;
int menusel=0;
int key_counter[6];
int ui_old[9];
int ui_oldpos=0;

void uiInit() {
  pinMode( B1,   INPUT );
  pinMode( B2,   INPUT );
  pinMode( B3,   INPUT );
  pinMode( B4,   INPUT );
  pinMode( B5,   INPUT );
  pinMode( B6,   INPUT );
  key_counter[0]=0;
  key_counter[1]=0;
  key_counter[2]=0;
  key_counter[3]=0;
  key_counter[4]=0;
  key_counter[5]=0;
  
  scantime = uiScantime();
  inttime = uiInttime_ms();
  
  ui_old[0]=samplerate;
  ui_old[1]=oversample;
  ui_old[2]=coldelay;
  ui_old[3]=scantime;
  ui_old[4]=param;
  ui_old[5]=inttime;
  ui_old[6]=0;
  ui_old[7]=0;
  ui_old[8]=0;
}

int scanKeys() {
  for( int i=0;i<6;i++ ) {
    int r=0;
    switch(i) {
      case 0: r=digitalRead(B1); break;
      case 1: r=digitalRead(B2); break;
      case 2: r=digitalRead(B3); break;
      case 3: r=digitalRead(B4); break;
      case 4: r=digitalRead(B5); break;
      case 5: r=digitalRead(B6); break;
    }
    if( !r ) {
      key_counter[i]++;
      if( key_counter[i]==20 ) {
        return i+1; 
      } else if( key_counter[i]>350 ) {
        return i+1;
      } else {
        return 0;
      }
    } else {
      key_counter[i]=0;
    }
  }
  return 0;
}


//  B1  IN    44  OK/ENTER
//  B2  IN    46  RIGHT
//  B3  IN    48  LEFT
//  B4  IN    50  DOWN
//  B5  IN    51  UP
//  B6  IN    52  CANCEL/BACK

int uiScantime() {
  int col_latency = coldelay;
  if( col_latency<30.0 ) col_latency=30.0;
  return 4190.0*(col_latency/1000.0+(oversample+1)*(1536.0)/(float)samplerate);
}

int uiInttime_ms() {
  return 1000.0*(oversample*(1536.0-18.0)/(float)samplerate);
}

void uiCreateSetup() {
  tft.fillScreen( ILI9340_BLACK );
  tft.drawLine( 0 , 36-2, 319, 36-2, ILI9340_WHITE );
  tft.drawLine( 0 , 51, 319, 51, ILI9340_WHITE );
  tft.setTextSize( 2 );
  
  if( param==0 )   tft.setTextColor( ILI9340_YELLOW );
  tft.setCursor( 0, 0  );
  tft.print( "SAMPLERATE" );
  tft.setCursor( 0, 18 );
  tft.print( samplerate );
  tft.print( "Hz" );
  if( param==0 )   tft.setTextColor( ILI9340_RED );
  
  if( param==1 )   tft.setTextColor( ILI9340_YELLOW );
  tft.setCursor( 125, 0  );
  tft.print( "OVERSAMPLE" );
  tft.setCursor( 125, 18 );
  tft.print( oversample );
  if( param==1 )   tft.setTextColor( ILI9340_RED );
  
  if( param==2 )   tft.setTextColor( ILI9340_YELLOW );
  tft.setCursor( 250, 0  );
  tft.print( "DELAY" );
  tft.setCursor( 250, 18  );
  tft.print( coldelay );
  tft.print( "ms" );
  if( param==2 )   tft.setTextColor( ILI9340_RED );
  
  tft.setTextColor( ILI9340_WHITE );
  
  tft.setCursor( 0, 36  );
  tft.print( scantime );
  tft.print( "s (" );
  tft.print( scantime/60 );
  tft.print( "m)" );
  
  tft.setCursor( 160, 36  );
  if( inttime<1000 ) {
    tft.print( inttime );
    tft.print( "ms" );
  } else {
    tft.print( inttime/1000 );
    tft.print( "s" );
  }

  uiDrawLightMeter();
}



void uiDrawSetup() {
  tft.setTextSize( 2 );

  if( ui_old[0]!=samplerate || param!=ui_old[4] ) {
    tft.setTextColor( ILI9340_BLACK );
    tft.setCursor( 0, 0  );
    tft.print( "SAMPLERATE" );
    tft.setCursor( 0, 18 );
    tft.print( ui_old[0] );
    tft.print( "Hz" );
    
    ui_old[0] = samplerate;
    
    if( param==0 )
      tft.setTextColor( ILI9340_YELLOW );
    else
      tft.setTextColor( ILI9340_RED );
    tft.setCursor( 0, 0  );
    tft.print( "SAMPLERATE" );      
    tft.setCursor( 0, 18 );
    tft.print( samplerate );
    tft.print( "Hz" );
  }
  
  if( ui_old[1]!=oversample || param!=ui_old[4] ) {
    tft.setTextColor( ILI9340_BLACK );
    tft.setCursor( 125, 0  );
    tft.print( "OVERSAMPLE" );    
    tft.setCursor( 125, 18 );
    tft.print( ui_old[1] );

    ui_old[1] = oversample;

    if( param==1 )
      tft.setTextColor( ILI9340_YELLOW );
    else
      tft.setTextColor( ILI9340_RED );
    tft.setCursor( 125, 0  );
    tft.print( "OVERSAMPLE" );      
    tft.setCursor( 125, 18 );
    tft.print( oversample );
  }
  
  if( ui_old[2]!=coldelay || param!=ui_old[4] ) {
    tft.setTextColor( ILI9340_BLACK );
    tft.setCursor( 250, 0  );
    tft.print( "DELAY" );
    tft.setCursor( 250, 18 );

    if( ui_old[2]<1000 ) {
      tft.print( ui_old[2] );
      tft.print( "ms" );
    } else {
      tft.print( ui_old[2]/1000 );
      tft.print( "s" );
    }    

    ui_old[2] = coldelay;

    if( param==2 )
      tft.setTextColor( ILI9340_YELLOW );
    else
      tft.setTextColor( ILI9340_RED );
    tft.setCursor( 250, 0  );
    tft.print( "DELAY" );      
    tft.setCursor( 250, 18 );
    if( coldelay<1000 ) {
      tft.print( coldelay );
      tft.print( "ms" );
    } else {
      tft.print( coldelay/1000 );
      tft.print( "s" );
    }       
  }

  if( scantime != ui_old[3] ) {
    tft.setTextColor( ILI9340_BLACK );
    //tft.setTextSize( 1 ); 
    tft.setCursor( 0, 36  );
    tft.print( ui_old[3] );
    tft.print( "s (" );
    tft.print( ui_old[3]/60 );
    tft.print( "m)" );    
    tft.setCursor( 160, 36  );    
    tft.print( ui_old[5] );
    tft.print( "ms" );
    
    ui_old[3] = scantime;
    ui_old[5] = inttime;
    
    tft.setTextColor( ILI9340_WHITE );
    tft.setCursor( 0, 36  );
    tft.print( scantime );
    tft.print( "s (" );
    tft.print( scantime/60 );
    tft.print( "m)" );
    tft.setCursor( 160, 36  );    
    tft.print( inttime );
    tft.print( "ms" );
  }
  ui_old[4] = param; 
}

int oldx[240];

void uiDrawLightMeter() {
  for( int y=56;y<239;y++) {
    tft.drawPixel( 20+oldx[y], y, ILI9340_BLACK );
    int index = (1536*(y-56))/(239-56);
    int s = (300*ccd_line[1535-index])/(4096*oversample);
    oldx[y]=s;
    tft.drawPixel( 20+s, y, ILI9340_WHITE );
    int c = (ccd_line[1535-index]>>4)/oversample;
    if(c>255)c=255;
    int color = tft.Color565( c, c, c );    
    tft.drawPixel( 10, y, color );  
    tft.drawPixel( 11, y, color );  
    tft.drawPixel( 12, y, color );  
    tft.drawPixel( 13, y, color );  
  }
}
  
void uiCreateAcquire() {
  tft.fillScreen( ILI9340_BLACK );
}

void uiRedrawPos() {
  tft.setTextColor( ILI9340_WHITE );
  tft.setCursor( 240, 36 );
  tft.print( step_pos*LCAM_XCONVF );
}

void uiDrawPos() {
 
  if( ui_oldpos!=step_pos ) {
    tft.setTextColor( ILI9340_BLACK );
    tft.setCursor( 240, 36 );
    tft.print( ui_oldpos*LCAM_XCONVF );

    ui_oldpos = step_pos;

    tft.setTextColor( ILI9340_WHITE ); 
    tft.setCursor( 240, 36 );
    tft.print( step_pos*LCAM_XCONVF );
  }
  ui_oldpos = step_pos; 
}


void uiCreateDarkframe() {
  tft.fillScreen( ILI9340_BLACK );
  tft.setTextSize( 2 );
  tft.setTextColor( ILI9340_YELLOW );
  
  tft.setCursor( 50, 50  );
  tft.print( "SAMPLERATE" );
  
  tft.setCursor( 150, 50  );
  tft.print( "COLUMN" );
}

void uiDrawDarkframe( int n ) {
  tft.setCursor( 50, 90  );
  tft.print( samplerate );
  tft.setCursor( 150, 90  );
  tft.print( n );
}

void uiDrawColumn() {
  int xp = 319-(320*camxpos)/LCAM_XSIZE;
  for( int y=0;y<239;y++) {
    int index = (1536*y)/(239);
    int s = ((ccd_line[1536-index]*16)/oversample)>>8;
    tft.drawPixel( xp, y, tft.Color565( s, s, s ) );
  }
  tft.drawPixel( xp, 0, tft.Color565( 255, 255, 255 ) );
  tft.drawPixel( xp, 239, tft.Color565( 255, 255, 255 ) );
}

void uiCreateMenu() {
  menusel = 0;
  tft.fillScreen( ILI9340_BLACK );
  tft.setTextSize( 2 );  
  tft.setCursor( 50, 40 );
  tft.setTextColor( ILI9340_YELLOW );
  tft.print("Capture dark frame");
  tft.setCursor( 50, 60 );
  tft.setTextColor( ILI9340_RED );
  tft.print("Capture gain cal.");
  tft.setCursor( 50, 80 );
  tft.setTextColor( ILI9340_RED );
  tft.print("Astronomy image");
  tft.setCursor( 50, 100 );
  tft.setTextColor( ILI9340_RED );
  tft.print("Exit menu");
}

void uiDrawMenu() {
  tft.setTextSize( 2 );  
  tft.setTextColor( ILI9340_RED );
  if( menusel==0 ) tft.setTextColor( ILI9340_YELLOW );
  tft.setCursor( 50, 40 );
  tft.print("Capture dark frame");
  tft.setTextColor( ILI9340_RED );
  if( menusel==1 ) tft.setTextColor( ILI9340_YELLOW );
  tft.setCursor( 50, 60 );
  tft.print("Capture gain cal.");
  tft.setTextColor( ILI9340_RED );
  if( menusel==2 ) tft.setTextColor( ILI9340_YELLOW );
  tft.setCursor( 50, 80 );
  tft.print("Astronomy image");
  tft.setTextColor( ILI9340_RED );
  if( menusel==3 ) tft.setTextColor( ILI9340_YELLOW );
  tft.setCursor( 50, 100 );
  tft.print("Exit menu");
}

void uiCreateProgress() {
  tft.drawLine( 0 , 235, 319, 235, ILI9340_WHITE );
  tft.drawLine( 0 , 239, 319, 239, ILI9340_WHITE );
  tft.drawLine( 0 , 235, 0, 239, ILI9340_WHITE );
  tft.drawLine( 319 , 235, 319, 239, ILI9340_WHITE );  
}

void uiDrawProgress( int percent ) {
  int x0 = 2+(ui_old[8]*315)/100;
  int x1 = 2+(percent*315)/100;
  if( x0<0) x0=0; else if( x0>319 ) x0=319;
  if( x1<0) x1=0; else if( x1>319 ) x1=319;
  tft.drawLine( x0 , 237, x1, 237, ILI9340_WHITE );
  ui_old[8] = percent;
}

void uiMenuAction( int item ) {
  switch( menusel ) {
    case 0:
    // Capture dark frames, one for each samplerate
    {
      int old_samplerate = samplerate;
      for( int sidx=0;sidx<7;sidx++ ) {
        
        switch( sidx ) {
          case 0: samplerate = 5000; break;
          case 1: samplerate = 10000; break;
          case 2: samplerate = 20000; break;
          case 3: samplerate = 40000; break;
          case 4: samplerate = 80000; break;
          case 5: samplerate = 160000; break;
          case 6: samplerate = 320000; break;
        }
        noInterrupts();
        for( int i=0;i<4;i++ ) {
          ccdAcquire( CCD_COPY );
        }
        ccdAcquire( CCD_COPY );
        for( int i=0;i<127;i++ ) {
          ccdAcquire( CCD_ADD );
        }
        interrupts();
        for( int i=0;i<1536;i++ ) {
          gDarkframe.ccd_line[sidx][i] = ((float)ccd_line[i])*(1.0/128.0);
        }
      }
      samplerate = old_samplerate;
      
      sdWriteDarkframe();
      
      ui_mode=SETUP;
      uiCreateSetup();
      uiDrawPos();
    }
    
    break;
    case 1:
    // Capture gain cal
    //
    // [ !!! ] Sensor must be *evenly illuminated*, at 30-50% saturation.
    //         Camera must be open to avoid correcting for vignetting.
    //         Light meter curve should be a straight line, the closer the better.
    //
    {
      sdReadGaincal();
      noInterrupts();  
      for( int i=0;i<4;i++ ) {
        ccdAcquire( CCD_COPY );
      }
      ccdAcquire( CCD_COPY );
      for( int i=0;i<127;i++ ) {
        ccdAcquire( CCD_ADD );
      }
      interrupts();  
      float favg=0.0;
      for( int i=0;i<1536;i++ ) {
        favg+=(ccd_line[i]-gDarkframe.ccd_line[samplerate_idx][i])/128.0;
      }
      favg*=(1.0/1536.0);
      for( int i=0;i<1536;i++ ) {
        // Calc gain to correct pixel:
        // (FL-FD) * g = favg
        // g = favg/(FL-FD)
        float s = ccd_line[i]>>7;
        float gain = favg/(s-gDarkframe.ccd_line[samplerate_idx][i]);
        gGaincal.gain[samplerate_idx][i] = (short)(gain*LCAM_GAIN_BASIS);
      }
      sdWriteGaincal();
      ui_mode=SETUP;
      uiCreateSetup();
      uiDrawPos();
    }
        
    break;
    case 2:
      
      // TODO:
      // Capture astronomy image
      //
      // Use current setup, begin capturing image columns without moving sensor
      // Display number of columns and wait for cancel
      //
      
    break;
    case 3:
      ui_mode=SETUP;
      uiCreateSetup();
      uiDrawPos();
    break;
  }
}

int ddraw=0; // delay draw counter

void uiRun() {
  int key = scanKeys();
  if( ui_mode==SETUP ) {
    if( key ) {
      
      if( key==2 ) {
        step_target++;
        if( ((++ddraw)&0x7F)==0 )
          uiDrawPos();
        else
          delay(1);
      }
      if( key==3 ) {
        step_target--;
        if( ((++ddraw)&0x7F)==0 )
          uiDrawPos();
        else
          delay(1);
      }
      
      if( key==3 ) if( param>0 )param--;
      if( key==2 ) if( param<2 )param++;
      switch( param ) {
        case 0:
          if( key==4 ) { 
            samplerate>>=1; 
            if( samplerate<5000 ) 
              samplerate=5000; 
            samplerate_idx--;
            if( samplerate_idx<0 )
              samplerate_idx=0;
          }
          if( key==5 ) {
            samplerate<<=1; 
            if( samplerate>320000 ) 
              samplerate=320000; 
            samplerate_idx++;
            if( samplerate_idx>6 )
              samplerate_idx=6;
          }
        break;
        case 1:
          if( key==4 ) { oversample--; if( oversample<1 ) oversample=1;   }
          if( key==5 ) { oversample++; if( oversample>8 ) oversample=8; }
        break;
        case 2:
          if( key==4 ) { coldelay>>=1; }
          if( key==5 ) { coldelay<<=1; if( coldelay==0 ) coldelay=1; if( coldelay>4096 ) coldelay=4096; }
        break;
      }
      if( key==1 ) {
        ui_mode=ACQUIRE;
        ccdAcquire( CCD_COPY );
        uiCreateAcquire();
        sdCreateImage();
        camxpos = step_pos/2;
        if( camxpos<0 ) camxpos=0;
      }
      if( key==6 && (key_counter[5]>350 ) ) {
        while( scanKeys() ) delay(4);
        ui_mode=MENU;
        uiCreateMenu();
      }
      if( ui_mode==SETUP ) {
        scantime = uiScantime();
        inttime = uiInttime_ms();
        uiDrawSetup();
      }
    } else {

      if( ((++ddraw)&127)==0 ) {
      
        // Capture and display one column:
        noInterrupts();  
        ccdAcquire(CCD_COPY);
        if( oversample>1 ) {
          for( int i=1;i<oversample;i++ ) {
            ccdAcquire( CCD_ADD );
          }
          interrupts();  
          ccdProcessColumn();
        } else {
          ccdAcquire( CCD_COPY );
          interrupts();  
          ccdProcessColumn();     
        }
        uiDrawLightMeter();
        // Display ccd position
        uiDrawPos();
      }
    }
  }
  
  /*
  Capturing image columns:
  1) Capture one column, throw away. This is necessary because integration time == time between two SI pulses, and we dont know how long SD+stepper operations will take
  2) Capture at least one more, save.
  
  Sequences for different amount of oversample:
  os  1    acq(0) acq(0) write
  os  2    acq(0) acq(0) acq(1) write
  os  3    acq(0) acq(0) acq(1) acq(1) write
  os  4    acq(0) acq(0) acq(1) acq(1) acq(1) write
  */
  
  if( ui_mode==ACQUIRE ) {
    // draw newest column
    
    if( (camxpos>=LCAM_XSIZE) || (key==6) ) {
      // Close image file, return ccd to origin
      sdCloseImage();
      gSettings.image_number++;
      sdWriteSettings();     
      stepZero();     
      ui_mode=SETUP;
      uiCreateSetup();
      uiDrawPos();
      uiDrawLightMeter();      
    } else {
      noInterrupts();  
      ccdAcquire( CCD_COPY );
      if( oversample>=2 ) {
        ccdAcquire( CCD_COPY );
        for( int i=0;i<oversample-1;i++ ) {
          ccdAcquire( CCD_ADD );
        }
        interrupts();  
        ccdProcessColumn();
        sdWriteImage( ccd_line, oversample );
      } else {
        ccdAcquire( CCD_COPY );
        interrupts();  
        ccdProcessColumn();
        sdWriteImage( ccd_line, oversample );
      }
      uiDrawColumn();
      delay( coldelay );
      camxpos++;
      step_target++;
      stepRun();
      //step_target++;
      //stepRun();
    }
  }

  if( ui_mode==MENU ) {
    delay(2);
    switch( key ) {    
      case 1: // ok
      uiMenuAction( menusel );
      break;
      case 2:
      break;
      case 3:
      break;
      case 4: // down
      menusel++;
      if( menusel>3 ) menusel=3;
      uiDrawMenu();
      break;
      case 5: // up
      menusel--;
      if( menusel<0 ) menusel=0;
      uiDrawMenu();
      break;
      case 6: // cancel
//      uiMenuAction( 3 );
      break;
    }
  }
}

/*
void printDirectory(File dir, int numTabs) {
   while(true) {
     
     File entry =  dir.openNextFile();
     if (! entry) {
       // no more files
       break;
     }
     for (uint8_t i=0; i<numTabs; i++) {
       //Serial.print('\t');
     }
     tft.print( entry.name() );
     if (entry.isDirectory()) {
       tft.print("/");
       printDirectory(entry, numTabs+1);
     } else {
       // files have sizes, directories do not
       tft.print("       ");
       tft.println(entry.size(), DEC);
     }
     entry.close();
   }
}
*/

void loop() {
  stepRun();
  uiRun();
}


