#include "Arduino.h"
#include "lcam.h"
#include "wifi.h"
#include <Adafruit_CC3000.h>
#include <ccspi.h>
#include <SPI.h>
#include <string.h>
#include "utility/debug.h"
#include "sdcard.h"

LCAM_SETTINGS pkt_settings;
LCAM_METER    pkt_meter;
LCAM_COLUMN   pkt_column;
LCAM_DONE     pkt_done;

// Use hardware SPI for the remaining pins
// On an UNO, SCK = 13, MISO = 12, and MOSI = 11
Adafruit_CC3000 cc3000 = Adafruit_CC3000(WIFI_CS, WIFI_IRQ, WIFI_EN,
                                         SPI_CLOCK_DIVIDER); // you can change this clock speed but DI

Adafruit_CC3000_Client cc3000client;
// !
// ! adafruit_cc3000.h changed SPI_CLOCK_DIVIDER from 6 to 16
// !

// Security can be WLAN_SEC_UNSEC, WLAN_SEC_WEP, WLAN_SEC_WPA or WLAN_SEC_WPA2
#define WLAN_SECURITY   WLAN_SEC_WPA2

void displayDriverMode(void);
uint16_t checkFirmwareVersion(void);
void displayMACAddress(void);
bool displayConnectionDetails(void);
void listSSIDResults(void);

#include <Adafruit_GFX.h>
#include <Adafruit_ILI9340.h>
extern Adafruit_ILI9340 tft;

boolean wifiInit() {
  // Setup pins
  pinMode( WIFI_EN,   OUTPUT );
  pinMode( WIFI_IRQ,    INPUT );
  pinMode( WIFI_CS,     OUTPUT );
  digitalWrite( WIFI_EN, LOW );
  digitalWrite( WIFI_CS, HIGH );  // power up HIGH
  
  // Initialize wifi module
  Serial.begin(115200);
  Serial.println(F("Hello, CC3000!\n")); 

//  displayDriverMode();
//  Serial.print("Free RAM: "); 
//  Serial.println(getFreeRam(), DEC);
  
  /* Initialise the module */
  //Serial.println(F("\nInitialising the CC3000 ..."));
  tft.println(F("CC3000: Initializing ..."));
  if (!cc3000.begin())
  {
    tft.println(F("CC3000: Initialization failed."));
    Serial.println(F("CC3000: Initialization failed."));
    return false;
  }

  /* Optional: Update the Mac Address to a known value */
/*
  uint8_t macAddress[6] = { 0x08, 0x00, 0x28, 0x01, 0x79, 0xB7 };
   if (!cc3000.setMacAddress(macAddress))
   {
     Serial.println(F("Failed trying to update the MAC address"));
     while(1);
   }
*/
  
  uint16_t firmware = checkFirmwareVersion();
  if (firmware < 0x113) {
    tft.println(F("CC3000: Wrong firmware revision."));
    Serial.println(F("CC3000: Wrong firmware revision."));
    return false;
  } 
  
  displayMACAddress();
  
  /* Optional: Get the SSID list (not available in 'tiny' mode) */
#ifndef CC3000_TINY_DRIVER
  listSSIDResults();
#endif
  
  /* Delete any old connection data on the module */
 // Serial.println(F("\nDeleting old connection profiles"));
  if (!cc3000.deleteProfiles()) {
    Serial.println(F("Failed!"));
    tft.println(F("CC3000: Failed deleting old profiles."));
    return false;
  }

  /* Optional: Set a static IP address instead of using DHCP.
     Note that the setStaticIPAddress function will save its state
     in the CC3000's internal non-volatile memory and the details
     will be used the next time the CC3000 connects to a network.
     This means you only need to call the function once and the
     CC3000 will remember the connection details.  To switch back
     to using DHCP, call the setDHCP() function (again only needs
     to be called once).
  */
  /*
  uint32_t ipAddress = cc3000.IP2U32(192, 168, 1, 19);
  uint32_t netMask = cc3000.IP2U32(255, 255, 255, 0);
  uint32_t defaultGateway = cc3000.IP2U32(192, 168, 1, 1);
  uint32_t dns = cc3000.IP2U32(8, 8, 4, 4);
  if (!cc3000.setStaticIPAddress(ipAddress, netMask, defaultGateway, dns)) {
    Serial.println(F("Failed to set static IP!"));
    while(1);
  }
  */
  /* Optional: Revert back from static IP addres to use DHCP.
     See note for setStaticIPAddress above, this only needs to be
     called once and will be remembered afterwards by the CC3000.
  */
  
  if (!cc3000.setDHCP()) {
    Serial.println(F("Failed to set DHCP!"));
    tft.println(F("CC3000: Failed to set DHCP mode."));
    while(1);
  }

  /* Attempt to connect to an access point */
 // char *ssid = WLAN_SSID;             /* Max 32 chars */
 // Serial.print(F("\nAttempting to connect to ")); Serial.println(ssid);
  tft.print(F("CC3000: SSID=")); tft.println(gNetwork.ssid);
  tft.print(F("CC3000: PASS=")); tft.println(gNetwork.password);
  
  /* NOTE: Secure connections are not available in 'Tiny' mode!
     By default connectToAP will retry indefinitely, however you can pass an
     optional maximum number of retries (greater than zero) as the fourth parameter.
  */
  if (!cc3000.connectToAP(gNetwork.ssid, gNetwork.password, WLAN_SECURITY, 3)) {
    Serial.println(F("Failed!")); 
    Serial.println(F("CC3000: Unable to connect to AP."));
    tft.println(F("CC3000: Unable to connect to AP."));
    return false;
//    while(1);
  }
  tft.println(F("CC3000: Connected to AP."));
   
//  Serial.println(F("Connected!"));
  
  // Wait for DHCP to complete 
  Serial.println(F("CC3000: Request DHCP..."));
  tft.println(F("CC3000: Request DHCP..."));
  int tmr = millis();
  while (!cc3000.checkDHCP())
  {
    delay(100); // ToDo: Insert a DHCP timeout!
    if( millis()-tmr > 10000 ) {
      Serial.println(F("CC3000: DHCP timed out."));
      tft.println(F("CC3000: DHCP timed out."));
      cc3000.disconnect();
      return false;
    }
  }  
  tft.println(F("CC3000: DHCP OK."));

  // Display the IP address DNS, Gateway, etc.
  while (! displayConnectionDetails()) {
    delay(1000);
  }
  
//  Serial.println("CC3000: Test OK, disconnecting...");
  /* You need to make sure to clean up after yourself or the CC3000 can freak out */
  /* the next time you try to connect ... */
//  cc3000.disconnect();  
  tft.println(F("CC3000: Network connection ready."));
  return true;
}

boolean wifiConnect() {
  // 1) Discover service
  //    * find AP ip
  uint32_t ipAddress, netmask, gateway, dhcpserv, dnsserv;
  if( cc3000.getIPAddress( &ipAddress, &netmask, &gateway, &dhcpserv, &dnsserv ) ) {
    cc3000client = connectTCP( gateway, 5750 );
    if( cc3000client.connected() ) {
      tft.println(F("CC3000: App connection ready."));
      return true;
    } else {
      tft.println(F("CC3000: TCP connection failed."));
    }
  }  
  return false;
}

boolean wifiStop() {
  cc3000.disconnect();
  return true;
}
  
 
boolean wifiTxSettings() {
  pkt_settings.cmd = 0xFFF0;
  pkt_settings.samplerate   = samplerate;
  pkt_settings.oversample   = oversample;
  pkt_settings.coldelay     = coldelay;
  pkt_settings.image_number = gSettings.image_number;
  cc3000client.write( (void*)pkt_settings, sizeof(pkt_settings), 0 );
  return true;
}

boolean wifiTxMeter() {
  pkt_settings.cmd = 0xFFF1;
// resample column:
  for( int i=0;i<512;i++ ) {
    pkt_settings.ccd_line[i] = (85*(ccd_line[i*3+0]+ccd_line[i*3+1]+ccd_line[i*3+2]))>>8;
  }
  cc3000client.write( (void*)pkt_meter, sizeof(pkt_meter), 0 );
  return true;
}

boolean wifiTxColumn() {
  pkt_column.cmd = 0xFFF2;
  // convert to short
  for( int i=0;i<1536;i++ ) {
    pkt_column.ccd_line[i] = ccd_line[i];
  }
  cc3000client.write( (void*)pkt_column, sizeof(pkt_column), 0 );
  return true;
}

boolean wifiTxDone() {
  pkt_done.cmd = 0xFFF3;
  cc3000client.write( (void*)pkt_done, sizeof(pkt_done), 0 );
  return true;
}
  
int wifiRxCommand() {

  return 0;
}
  
  

/**************************************************************************/
/*!
    @brief  Displays the driver mode (tiny of normal), and the buffer
            size if tiny mode is not being used

    @note   The buffer size and driver mode are defined in cc3000_common.h
*/
/**************************************************************************/
void displayDriverMode(void)
{
  #ifdef CC3000_TINY_DRIVER  
    Serial.println(F("CC3000: Configured in 'Tiny' mode"));  
  #else  
    Serial.print(F("RX Buffer : "));
    Serial.print(CC3000_RX_BUFFER_SIZE);
    Serial.print(F(" bytes"));
    Serial.print(F("TX Buffer : "));
    Serial.print(CC3000_TX_BUFFER_SIZE);
    Serial.println(F(" bytes"));
  #endif
}

/**************************************************************************/
/*!
    @brief  Tries to read the CC3000's internal firmware patch ID
*/
/**************************************************************************/
uint16_t checkFirmwareVersion(void)
{
  uint8_t major, minor;
  uint16_t version;
  
#ifndef CC3000_TINY_DRIVER  
  if(!cc3000.getFirmwareVersion(&major, &minor))
  {  
    Serial.println(F("Unable to retrieve the firmware version!\r\n"));
    version = 0;
  }
  else
  {  
    Serial.print(F("Firmware V. : "));
    Serial.print(major); Serial.print(F(".")); Serial.println(minor);
    version = major; version <<= 8; version |= minor; 
  }
#endif
  return version;
}

/**************************************************************************/
/*!
    @brief  Tries to read the 6-byte MAC address of the CC3000 module
*/
/**************************************************************************/
void displayMACAddress(void)
{
  uint8_t macAddress[6];
  
  if(!cc3000.getMacAddress(macAddress))
  {   
    Serial.println(F("Unable to retrieve MAC Address!\r\n"));   
  }
  else
  {  
    Serial.print(F("MAC Address : ")); 
    cc3000.printHex((byte*)&macAddress, 6);
  }
}


/**************************************************************************/
/*!
    @brief  Tries to read the IP address and other connection details
*/
/**************************************************************************/
bool displayConnectionDetails(void)
{
  uint32_t ipAddress, netmask, gateway, dhcpserv, dnsserv;
  
  if(!cc3000.getIPAddress(&ipAddress, &netmask, &gateway, &dhcpserv, &dnsserv))
  {  
    Serial.println(F("Unable to retrieve the IP Address!\r\n")); 
    tft.println(F("Unable to retrieve the IP Address!\r\n"));
    return false;
  }
  else
  {  
    Serial.print(F("\nIP Addr: ")); cc3000.printIPdotsRev(ipAddress);
    Serial.print(F("\nNetmask: ")); cc3000.printIPdotsRev(netmask);
    Serial.print(F("\nGateway: ")); cc3000.printIPdotsRev(gateway);
    Serial.print(F("\nDHCPsrv: ")); cc3000.printIPdotsRev(dhcpserv);
    Serial.print(F("\nDNSserv: ")); cc3000.printIPdotsRev(dnsserv);
    Serial.println();    
    return true;
  }
}

/**************************************************************************/
/*!
    @brief  Begins an SSID scan and prints out all the visible networks
*/
/**************************************************************************/

void listSSIDResults(void)
{
  uint8_t valid, rssi, sec, index;
  char ssidname[33]; 

  index = cc3000.startSSIDscan();
   
  Serial.print(F("Networks found: ")); Serial.println(index);
  Serial.println(F("================================================"));

  while (index) {
    index--;

    valid = cc3000.getNextSSID(&rssi, &sec, ssidname);
  
    Serial.print(F("SSID Name    : ")); Serial.print(ssidname);
    Serial.println();
    Serial.print(F("RSSI         : "));
    Serial.println(rssi);
    Serial.print(F("Security Mode: "));
    Serial.println(sec);
    Serial.println();
  } 
  Serial.println(F("================================================")); 

  cc3000.stopSSIDscan();
}
  


