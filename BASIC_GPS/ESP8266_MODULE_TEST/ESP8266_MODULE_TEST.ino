/* 
 * Author: EmbedCrest Technology Private Limited
 */

#include <TinyGPS++.h>
#include <SoftwareSerial.h>

// -------------------------------------------------------
// GPS wiring & configuration
// -------------------------------------------------------
static const int GPS_RX_PIN = 5;
static const int GPS_TX_PIN = 16;
static const uint32_t GPS_BAUDRATE = 9600;

// TinyGPS++ instance
TinyGPSPlus gpsCore;

// UART bridge to GPS module
SoftwareSerial gpsSerial(GPS_RX_PIN, GPS_TX_PIN);

// -------------------------------------------------------
// Forward declarations
// -------------------------------------------------------
void printGpsSnapshot();

// -------------------------------------------------------
// Notification handler
// setup()
// -------------------------------------------------------
void setup()
{
  Serial.begin(115200);
  gpsSerial.begin(GPS_BAUDRATE);

  Serial.println(F("DeviceExample.ino"));
  Serial.println(F("A simple demonstration of TinyGPS++ with an attached GPS module"));
  Serial.print(F("Testing TinyGPS++ library v. "));
  Serial.println(TinyGPSPlus::libraryVersion());
  Serial.println(F("by Mikal Hart"));
  Serial.println();
}

// -------------------------------------------------------
// Main loop
// -------------------------------------------------------
void loop()
{
  // Feed GPS parser
  while (gpsSerial.available() > 0)
  {
    if (gpsCore.encode(gpsSerial.read()))
    {
      printGpsSnapshot();
    }
  }

  // Safety: no GPS data received for 5 seconds
  if (millis() > 5000 && gpsCore.charsProcessed() < 10)
  {
    Serial.println(F("No GPS detected: check wiring."));
    while (true) { /* halt */ }
  }
}

// -------------------------------------------------------
// Notification handler
// Print decoded GPS snapshot
// -------------------------------------------------------
void printGpsSnapshot()
{
  Serial.print(F("Location: "));
  if (gpsCore.location.isValid())
  {
    Serial.print(gpsCore.location.lat(), 6);
    Serial.print(F(","));
    Serial.print(gpsCore.location.lng(), 6);
  }
  else
  {
    Serial.print(F("INVALID"));
  }

  Serial.print(F("  Date/Time: "));
  if (gpsCore.date.isValid())
  {
    Serial.print(gpsCore.date.month());
    Serial.print(F("/"));
    Serial.print(gpsCore.date.day());
    Serial.print(F("/"));
    Serial.print(gpsCore.date.year());
  }
  else
  {
    Serial.print(F("INVALID"));
  }

  Serial.print(F(" "));
  if (gpsCore.time.isValid())
  {
    if (gpsCore.time.hour() < 10)   Serial.print(F("0"));
    Serial.print(gpsCore.time.hour());
    Serial.print(F(":"));

    if (gpsCore.time.minute() < 10) Serial.print(F("0"));
    Serial.print(gpsCore.time.minute());
    Serial.print(F(":"));

    if (gpsCore.time.second() < 10) Serial.print(F("0"));
    Serial.print(gpsCore.time.second());
    Serial.print(F("."));

    if (gpsCore.time.centisecond() < 10) Serial.print(F("0"));
    Serial.print(gpsCore.time.centisecond());
  }
  else
  {
    Serial.print(F("INVALID"));
  }

  Serial.println();
}
