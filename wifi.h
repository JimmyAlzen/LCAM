#ifndef __WIFI_H__
#define __WIFI_H__

//////////////////////////////////////////////////////////////////////////////////
// WIFI Interface for Arduino - Andriod realtime channel
// Written 2014 by Jimmy Alzén
//////////////////////////////////////////////////////////////////////////////////

#pragma pack( push, 1 )

typedef struct LCAM_SETTINGS {
  short cmd;// = 0xFFF0;
  short samplerate;  // Hz
  short oversample;  // Times
  short coldelay;    // milliseconds
  short image_number;// 0000-9999
} LCAM_SETTINGS;

typedef struct LCAM_METER {
  short cmd;// = 0xFFF1;
  short ccd_line[512];
} LCAM_METER;

typedef struct LCAM_COLUMN {
  short cmd;// = 0xFFF2;
  short pos;
  short ccd_line[1536];
} LCAM_COLUMN;

typedef struct LCAM_DONE {
  short cmd;// = 0xFFF3;
} LCAM_DONE;

typedef struct LCAM_COMMAND {
  short cmd; // = 0xFFF4;
  short cam_cmd;
  short cam_param1;
  short cam_param2;
  char  cam_paramstr[32];
}LCAM_COMMAND;

#pragma pack( pop )

extern LCAM_SETTINGS pkt_settings;
extern LCAM_METER    pkt_meter;
extern LCAM_COLUMN   pkt_column;
extern LCAM_DONE     pkt_done;

// Network protocol:
//
// IDLE/METER mode
//  LCAM_SETTINGS is sent on settings changes
//  LCAM_METER is sent 2-5 times per second
//
// ACQUIRE mode
//  LCAM_COLUMN is sent for every image column (about 4190 in all)
//
// COMMANDS
//  LCAM_COMMAND is sent for every command to Camera
//
// Connection
//  Android phone's IP is found via broadcast ping on local subnet
//  
//  Camera connects to Android phone's TCP socket on port 5750
//

boolean wifiInit();
boolean wifiConnect();
boolean wifiStop();
boolean wifiTxSettings();
boolean wifiTxMeter();
boolean wifiTxColumn();
boolean wifiTxDone();
int wifiRxCommand();

#endif //__WIFI_H__
