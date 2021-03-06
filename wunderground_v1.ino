#include <Adafruit_CC3000.h>
#include <ccspi.h>
#include <SPI.h>
#include <string.h>
#include "utility/debug.h"

// These are the interrupt and control pins
#define ADAFRUIT_CC3000_IRQ   3  // MUST be an interrupt pin!
// These can be any two pins
#define ADAFRUIT_CC3000_VBAT  5
#define ADAFRUIT_CC3000_CS    10
// Use hardware SPI for the remaining pins
// On an UNO, SCK = 13, MISO = 12, and MOSI = 11
Adafruit_CC3000 cc3000 = Adafruit_CC3000(ADAFRUIT_CC3000_CS, ADAFRUIT_CC3000_IRQ, ADAFRUIT_CC3000_VBAT,
                                         SPI_CLOCK_DIVIDER); // you can change this clock speed

#define WLAN_SSID       "xxx"           // cannot be longer than 32 characters!
#define WLAN_PASS       "yyy"
// Security can be WLAN_SEC_UNSEC, WLAN_SEC_WEP, WLAN_SEC_WPA or WLAN_SEC_WPA2
#define WLAN_SECURITY   WLAN_SEC_WPA2
uint32_t ipAddress = cc3000.IP2U32(192, 168, 1, 12);
uint32_t netMask = cc3000.IP2U32(255, 255, 255, 0);
uint32_t defaultGateway = cc3000.IP2U32(192, 168, 1, 1);
uint32_t dns = cc3000.IP2U32(8, 8, 4, 4);

#define WEB_IDLE_TIMEOUT_MS  9999  // Amount of time to wait (in milliseconds) with no data 
                                   // received before closing the connection.  If you know the server
                                   // you're accessing is quick to respond, you can reduce this value.

// What page to grab!
#define WEBSITE      "api.wunderground.com"
#define WEBPAGE      "/api/e115799858b697fc/tide/q/CA/Kailua_Kona.json"

/**************************************************************************/
/*!
    @brief  Sets up the HW and the CC3000 module (called automatically
            on startup)
*/
/**************************************************************************/

uint32_t ip;

void setup(void)
{
  Serial.begin(115200);
  Serial.println(F("==============")); 
  Serial.println(F("Hello, CC3000!")); 

  Serial.print("Free RAM: "); Serial.println(getFreeRam(), DEC);
  
  /* Initialise the module */
  Serial.println(F("Initializing..."));
//  cc3000.begin();
//  resetCC3000();
  if (!cc3000.begin(0))
  {
    Serial.println(F("Couldn't start the CC3000. Check your wiring?"));
    while(1);
  }
  /* Setting MAC address */
//  uint8_t macAddress[6] = { 0x08, 0x00, 0x28, 0x01, 0x79, 0xB7 };
  Serial.println(F("Getting MAC address"));
  uint8_t macAddress[6];
  if (cc3000.getMacAddress(macAddress)) {
    Serial.print(F("MAC address: "));
    cc3000.printHexChar(macAddress, 6);
  }

  Serial.println(F("Getting CC3000 firmware version"));
  uint8_t CC3000_firmware_version[2];
  if (cc3000.getFirmwareVersion(&CC3000_firmware_version[0], &CC3000_firmware_version[1])) {
    Serial.print(F("Firmware version: "));Serial.print(CC3000_firmware_version[0]); Serial.print(F(".")); Serial.println(CC3000_firmware_version[1]);
  }

  Serial.println(F("Deleting profiles"));
  if (!cc3000.deleteProfiles()) {
    Serial.println(F("Failed"));
  }
  // Optional SSID scan
   listSSIDResults();

//  Serial.println(F("Setting IP address"));
//  setStaticIPaddress();
  
  Serial.print(F("Connecting to ")); Serial.println(WLAN_SSID);
  if (!cc3000.connectToAP(WLAN_SSID, WLAN_PASS, WLAN_SECURITY)) {
    Serial.println(F("Failed!"));
    while(1);
  }
  boolean isConnected = cc3000.checkConnected();
  if (isConnected) {
    Serial.println(F("Connected!"));
  } else {
    Serial.println(F("Connection Failed"));
    resetCC3000();
    return;
  }
 
  /* Wait for DHCP to complete */
  Serial.println(F("Requesting DHCP"));
  int iLoopCount = 0;
  int iRet = 0;
  for (iLoopCount = 0; iLoopCount < 10; iLoopCount++) {
    iRet = cc3000.checkDHCP();
    if (iRet) {
      break;
    }
    delay(1000);
  }
  if (!iRet) {
    Serial.println(F("DHCP Failed"));
    resetCC3000();
    return;
  }

  /* Display the IP address DNS, Gateway, etc. */
  for (iLoopCount = 0; iLoopCount < 10; iLoopCount++) {
    iRet = displayConnectionDetails();
    if (iRet) {
      break;
    } else {
      delay(1000);
    }
  }
  if (!iRet) {
    Serial.println(F("Could not get connection details"));
    resetCC3000();
    return;
  }

  ip = 0;
  // Try looking up the website's IP address
  Serial.print(WEBSITE); Serial.print(F(" -> "));
  for (iLoopCount = 0; iLoopCount < 10; iLoopCount++) {
    iRet = cc3000.getHostByName(WEBSITE, &ip);
    if (!iRet) {
      Serial.println(F("Couldn't resolve!"));
    }
    delay(500);
  }
  if (ip == 0) {
    Serial.println(F("Could not get website IP address"));
    resetCC3000();
    return;
  }
  
  cc3000.printIPdotsRev(ip);
  Serial.println(F(""));
  
  // Optional: Do a ping test on the website
  /*
  Serial.print(F("\n\rPinging ")); cc3000.printIPdotsRev(ip); Serial.print("...");  
  replies = cc3000.ping(ip, 5);
  Serial.print(replies); Serial.println(F(" replies"));
  */  

  /* Try connecting to the website.
     Note: HTTP/1.1 protocol is used to keep the server from closing the connection before all data is read.
  */
  Serial.println(F("Making web services call"));
  
  Adafruit_CC3000_Client www = cc3000.connectTCP(ip, 80);
  if (www.connected()) {
    www.fastrprint(F("GET "));
    www.fastrprint(WEBPAGE);
    www.fastrprint(F(" HTTP/1.1\r\n"));
    www.fastrprint(F("Host: ")); www.fastrprint(WEBSITE); www.fastrprint(F("\r\n"));
    www.fastrprint(F("\r\n"));
    www.println();
  } else {
    Serial.println(F("Connection failed"));    
    resetCC3000();
    return;
  }

  Serial.println(F("-------------------------------------"));
  
  /* Read data until either the connection is closed, or the idle timeout is reached. */ 
  unsigned long lastRead = millis();
  while (www.connected() && (millis() - lastRead < WEB_IDLE_TIMEOUT_MS)) {
    while (www.available()) {
      char c = www.read();
      Serial.print(c);
      lastRead = millis();
    }
  }
  www.close();
  Serial.println(F("\n-------------------------------------"));
  
  /* You need to make sure to clean up after yourself or the CC3000 can freak out */
  /* the next time your try to connect ... */
  Serial.println(F("\n\nDisconnecting"));
  resetCC3000();
}

/**************************************************************************/
/*!
    @brief  Begins an SSID scan and prints out all the visible networks
*/
/**************************************************************************/

void listSSIDResults(void)
{
  uint32_t index;
  uint8_t valid, rssi, sec;
  char ssidname[33]; 

  if (!cc3000.startSSIDscan(&index)) {
    Serial.println(F("SSID scan failed!"));
    return;
  }

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

void setStaticIPaddress(void) {
  if (!cc3000.setStaticIPAddress(ipAddress, netMask, defaultGateway, dns)) {
    Serial.println(F("Failed to set static IP!"));
    while(1);
  }
}

void resetCC3000(void) {
  cc3000.disconnect();
  cc3000.reboot();
  cc3000.stop();
  digitalWrite(ADAFRUIT_CC3000_VBAT, LOW);
  delay(20);
  digitalWrite(ADAFRUIT_CC3000_VBAT, HIGH);
  Serial.println(F("CC3000 reset"));
}

void loop(void) {
  delay(1000);
}

