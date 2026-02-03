/* 
 * Author: EmbedCrest Technology Private Limited
 */
#include <TinyGPS++.h>
#include <SoftwareSerial.h>
#include <TimeLib.h>
#include <SPI.h>
#include <TFT_eSPI.h>

// -------------------------------------------------------
// Display & GPS wiring
// -------------------------------------------------------
static const int GPS_RX_PIN = 5;
static const int GPS_TX_PIN = 16;
static const uint32_t GPS_BAUDRATE = 9600;
static const int TIMEZONE_OFFSET_HOURS = -8;   // Pacific Standard Time

TFT_eSPI      gpsTft = TFT_eSPI();
TinyGPSPlus   gpsParser;
SoftwareSerial gpsPort(GPS_RX_PIN, GPS_TX_PIN);

// -------------------------------------------------------
// Time / position state
// -------------------------------------------------------
time_t lastClockUpdate = 0;
bool   hasLockOnGps    = false;

char  timeText[16];
char  dateText[16];

float lastLat = 0.0f;
float lastLon = 0.0f;

// -------------------------------------------------------
// Forward declarations
// -------------------------------------------------------
void showStartupScreen();
void showErrorScreen();
void syncClockFromGps();
void drawDateTime();
void drawLocationIfChanged();

// -------------------------------------------------------
// Notification handler
// setup()
// -------------------------------------------------------
void setup()
{
  Serial.begin(115200);
  gpsPort.begin(GPS_BAUDRATE);

  gpsTft.begin();
  gpsTft.setRotation(3);

  showStartupScreen();
}

// -------------------------------------------------------
// Main polling loop
// -------------------------------------------------------
void loop()
{
  // -----------------------------------------------------
  // Feed GPS parser with all available bytes
  // -----------------------------------------------------
  while (gpsPort.available() > 0)
  {
    char c = gpsPort.read();
    if (gpsParser.encode(c))
    {
      // First valid sentence switches to black main screen
      if (!hasLockOnGps)
      {
        hasLockOnGps = true;
        gpsTft.fillScreen(TFT_BLACK);
      }

      syncClockFromGps();
      drawLocationIfChanged();
    }
  }

  // -----------------------------------------------------
  // Refresh time/date once per second when clock is valid
  // -----------------------------------------------------
  if (timeStatus() != timeNotSet)
  {
    time_t nowTs = now();
    if (nowTs != lastClockUpdate)
    {
      lastClockUpdate = nowTs;
      drawDateTime();
    }
  }

  // -----------------------------------------------------
  // GPS presence watchdog
  // -----------------------------------------------------
  if (millis() > 5000 && gpsParser.charsProcessed() < 10)
  {
    Serial.println(F("No GPS detected: check wiring."));
    showErrorScreen();
    while (true) { /* halt */ }
  }
}

// -------------------------------------------------------
// Screen helpers
// -------------------------------------------------------
void showStartupScreen()
{
  gpsTft.fillScreen(TFT_WHITE);
  gpsTft.setTextSize(1);
  gpsTft.setTextColor(TFT_RED, TFT_WHITE);
  gpsTft.setTextDatum(TC_DATUM);
  gpsTft.drawString("Waiting for GPS...", 240, 140, 4);
}

void showErrorScreen()
{
  gpsTft.fillScreen(TFT_RED);
  gpsTft.setTextSize(1);
  gpsTft.setTextColor(TFT_WHITE, TFT_RED);
  gpsTft.setTextDatum(TC_DATUM);
  gpsTft.drawString("No GPS detected: Check Wiring!", 240, 140, 4);
}

// -------------------------------------------------------
// Time & location drawing
// -------------------------------------------------------
void syncClockFromGps()
{
  if (!gpsParser.date.isValid() || !gpsParser.time.isValid())
    return;

  int  year   = gpsParser.date.year();
  byte month  = gpsParser.date.month();
  byte day    = gpsParser.date.day();
  byte hour   = gpsParser.time.hour();
  byte minute = gpsParser.time.minute();
  byte second = gpsParser.time.second();

  setTime(hour, minute, second, day, month, year);
  adjustTime(TIMEZONE_OFFSET_HOURS * SECS_PER_HOUR);
}

// -------------------------------------------------------
// Notification handler
// -------------------------------------------------------
void drawLocationIfChanged()
{
  if (!gpsParser.location.isValid())
    return;

  gpsTft.setTextDatum(TR_DATUM);
  gpsTft.setTextSize(2);
  gpsTft.setTextColor(TFT_WHITE, TFT_BLACK);

  float lat = gpsParser.location.lat();
  float lon = gpsParser.location.lng();

  if (lat != lastLat)
  {
    gpsTft.drawString(String(lat, 6), 460, 120, 4);
    lastLat = lat;
  }

  if (lon != lastLon)
  {
    // Note: same coordinates as original sketch
    gpsTft.drawString(String(lon, 6), 460, 120, 4);
    lastLon = lon;
  }
}

void drawDateTime()
{
  gpsTft.setTextDatum(TL_DATUM);
  gpsTft.setTextSize(1);
  gpsTft.setTextColor(TFT_WHITE, TFT_BLACK);

  sprintf(timeText, "Time: %02u:%02u:%02u", hour(), minute(), second());
  gpsTft.drawString(timeText, 0, 0, 4);

  sprintf(dateText, "Date: %02u/%02u/%04u", month(), day(), year());
  gpsTft.drawString(dateText, 0, 26, 4);
}
