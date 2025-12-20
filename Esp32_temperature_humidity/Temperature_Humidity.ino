/* 
 * Author: EmbedCrest Technology Private Limited
 */
#include "SPIFFS.h"

#include <JPEGDecoder.h>

#include <Adafruit_GFX.h>
#include <Adafruit_SSD1351.h>
#include <Adafruit_Sensor.h>
#include <DHT.h>
#include <DHT_U.h>

// -------------------------------------------------------
// Hardware configuration
// -------------------------------------------------------
#define PIN_DHT_DATA   19
#define DHT_MODEL      DHT11

#define OLED_WIDTH     128
#define OLED_HEIGHT    128

#define OLED_SCLK_PIN  18
#define OLED_MOSI_PIN  23
#define OLED_DC_PIN    17
#define OLED_CS_PIN    5
#define OLED_RST_PIN   4

// -------------------------------------------------------
// Color table
// -------------------------------------------------------
#define CLR_BLACK   0x0000
#define CLR_WHITE   0xFFFF

// -------------------------------------------------------
// File paths in SPIFFS
// -------------------------------------------------------
const char *PATH_ICON_TEMP    = "/temperature.jpg";
const char *PATH_ICON_HUMID   = "/humidity.jpg";

// -------------------------------------------------------
// Global objects
// -------------------------------------------------------
DHT_Unified          g_dht(PIN_DHT_DATA, DHT_MODEL);
Adafruit_SSD1351     g_oled(OLED_WIDTH, OLED_HEIGHT,
                            OLED_CS_PIN, OLED_DC_PIN,
                            OLED_MOSI_PIN, OLED_SCLK_PIN,
                            OLED_RST_PIN);

uint32_t g_sampleDelayMs = 1000;
int      g_lastTempC     = INT16_MIN;
int      g_lastHumidity  = INT16_MIN;

// -------------------------------------------------------
// Utility helpers
// -------------------------------------------------------
static inline uint32_t u32_min(uint32_t a, uint32_t b)
{
  return (a < b) ? a : b;
}

// -------------------------------------------------------
// Forward declarations
// -------------------------------------------------------
void initOledLayout();
void showTemperatureValue(int tempC);
void showHumidityValue(int humidity);
void drawJpegFromSpiffs(const char *path, int x, int y);
void renderDecodedJpeg(int xOffset, int yOffset);
void printJpegInfo();
void makeJpegArray(const char *filename);

// -------------------------------------------------------
// Notification handler
// -------------------------------------------------------
void setup()
{
  Serial.begin(115200);

  // -------------------------------------------------------
  // Init SPIFFS
  // -------------------------------------------------------
  if (!SPIFFS.begin()) {
    Serial.println(F("SPIFFS mount failed"));
    while (true) { yield(); }
  }

  // -------------------------------------------------------
  // Init OLED
  // -------------------------------------------------------
  g_oled.begin();
  g_oled.fillRect(0, 0, OLED_WIDTH, OLED_HEIGHT, CLR_WHITE);

  // -------------------------------------------------------
  // Init DHT sensor (temperature + humidity)
// -------------------------------------------------------
  g_dht.begin();
  sensor_t sensorInfo;
  g_dht.temperature().getSensor(&sensorInfo);
  g_sampleDelayMs = sensorInfo.min_delay / 1000;

  // -------------------------------------------------------
  // Draw static UI (icons + placeholders)
// -------------------------------------------------------
  initOledLayout();
}

// -------------------------------------------------------
// Main loop – periodic sensor acquisition
// -------------------------------------------------------
void loop()
{
  delay(g_sampleDelayMs);

  sensors_event_t evt;

  // -------------------------------------------------------
  // Read temperature
  // -------------------------------------------------------
  g_dht.temperature().getEvent(&evt);
  if (isnan(evt.temperature)) {
    Serial.println(F("Temperature read failed"));
  } else {
    int tC = (int)evt.temperature;
    Serial.print(F("Temperature: "));
    Serial.print(tC);
    Serial.println(F(" °C"));
    showTemperatureValue(tC);
  }

  // -------------------------------------------------------
  // Read humidity
  // -------------------------------------------------------
  g_dht.humidity().getEvent(&evt);
  if (isnan(evt.relative_humidity)) {
    Serial.println(F("Humidity read failed"));
  } else {
    int h = (int)evt.relative_humidity;
    Serial.print(F("Humidity   : "));
    Serial.print(h);
    Serial.println(F(" %"));
    showHumidityValue(h);
  }
}

// -------------------------------------------------------
// Screen layout and dynamic value updates
// -------------------------------------------------------
void initOledLayout()
{
  // Icons
  drawJpegFromSpiffs(PATH_ICON_TEMP,  0,  0);
  drawJpegFromSpiffs(PATH_ICON_HUMID, 0, 64);

  // Initial placeholders
  g_oled.setTextColor(CLR_BLACK);
  g_oled.setTextSize(3);

  g_oled.setCursor(72, 24);
  g_oled.print("?");

  g_oled.setCursor(72, 88);
  g_oled.print("?");
}

// -------------------------------------------------------
// Notification handler
// -------------------------------------------------------
void showTemperatureValue(int tempC)
{
  if (tempC == g_lastTempC) {
    return;
  }

  g_oled.fillRect(64, 0, 64, 64, CLR_WHITE);
  g_oled.setTextColor(CLR_BLACK);
  g_oled.setTextSize(3);
  g_oled.setCursor(70, 24);

  String txt = String(tempC) + "C";
  g_oled.print(txt);

  g_lastTempC = tempC;
}

// -------------------------------------------------------
// Notification handler
// -------------------------------------------------------
void showHumidityValue(int humidity)
{
  if (humidity == g_lastHumidity) {
    return;
  }

  g_oled.fillRect(64, 64, 64, 64, CLR_WHITE);
  g_oled.setTextColor(CLR_BLACK);
  g_oled.setTextSize(3);
  g_oled.setCursor(70, 88);

  String txt = String(humidity) + "%";
  g_oled.print(txt);

  g_lastHumidity = humidity;
}

// -------------------------------------------------------
// JPEG loader for SPIFFS files
// -------------------------------------------------------
void drawJpegFromSpiffs(const char *path, int x, int y)
{
  Serial.println(F("====================================="));
  Serial.print  (F("Render JPEG: "));
  Serial.println(path);
  Serial.println(F("====================================="));

  fs::File f = SPIFFS.open(path, "r");
  if (!f) {
    Serial.print(F("File not found: "));
    Serial.println(path);
    return;
  }
  f.close(); // Decoder will reopen by name

  bool ok = JpegDec.decodeFsFile(path);
  if (!ok) {
    Serial.println(F("Unsupported JPEG format"));
    return;
  }

  printJpegInfo();
  renderDecodedJpeg(x, y);
}

// -------------------------------------------------------
// JPEG MCU block renderer
// -------------------------------------------------------
void renderDecodedJpeg(int xOffset, int yOffset)
{
  uint16_t  mcuW   = JpegDec.MCUWidth;
  uint16_t  mcuH   = JpegDec.MCUHeight;
  uint32_t  imgW   = JpegDec.width;
  uint32_t  imgH   = JpegDec.height;

  uint32_t  edgeW  = u32_min(mcuW, imgW % mcuW);
  uint32_t  edgeH  = u32_min(mcuH, imgH % mcuH);

  uint32_t  blockW = mcuW;
  uint32_t  blockH = mcuH;

  uint32_t  tStart = millis();

  uint32_t  limitX = imgW + xOffset;
  uint32_t  limitY = imgH + yOffset;

  while (JpegDec.read()) {
    uint16_t *pixels = JpegDec.pImage;

    int32_t x = JpegDec.MCUx * mcuW + xOffset;
    int32_t y = JpegDec.MCUy * mcuH + yOffset;

    // Adjust block dimensions on right / bottom edges
    blockW = (x + mcuW <= (int32_t)limitX) ? mcuW : edgeW;
    blockH = (y + mcuH <= (int32_t)limitY) ? mcuH : edgeH;

    // Compress last column for right edge
    if (blockW != mcuW) {
      for (uint32_t row = 1; row < blockH - 1; ++row) {
        memcpy(pixels + row * blockW, pixels + (row + 1) * mcuW, blockW * 2);
      }
    }

    // Draw block if still on screen
    if ((x + blockW) <= g_oled.width() && (y + blockH) <= g_oled.height()) {
      g_oled.drawRGBBitmap(x, y, pixels, blockW, blockH);
    } else if ((y + blockH) >= g_oled.height()) {
      // Abort when image passes bottom edge
      JpegDec.abort();
    }
  }

  uint32_t elapsed = millis() - tStart;
  Serial.print  (F("JPEG render time: "));
  Serial.print  (elapsed);
  Serial.println(F(" ms"));
  Serial.println(F("====================================="));
}

// -------------------------------------------------------
// JPEG decoder information dump
// -------------------------------------------------------
void printJpegInfo()
{
  Serial.println(F("JPEG info ---------------------------"));
  Serial.print  (F("Width      : ")); Serial.println(JpegDec.width);
  Serial.print  (F("Height     : ")); Serial.println(JpegDec.height);
  Serial.print  (F("Components : ")); Serial.println(JpegDec.comps);
  Serial.print  (F("MCU / row  : ")); Serial.println(JpegDec.MCUSPerRow);
  Serial.print  (F("MCU / col  : ")); Serial.println(JpegDec.MCUSPerCol);
  Serial.print  (F("Scan type  : ")); Serial.println(JpegDec.scanType);
  Serial.print  (F("MCU width  : ")); Serial.println(JpegDec.MCUWidth);
  Serial.print  (F("MCU height : ")); Serial.println(JpegDec.MCUHeight);
  Serial.println(F("------------------------------------"));
}

// -------------------------------------------------------
// Helper: dump JPEG file as C array to Serial
// (not used in normal runtime, kept as tool)
// -------------------------------------------------------
void makeJpegArray(const char *filename)
{
  fs::File jpgFile = SPIFFS.open(filename, "r");
  if (!jpgFile) {
    Serial.println(F("JPEG file not found"));
    return;
  }

  Serial.println(F("// Generated by JPEGDecoder:"));
  Serial.println(F("#if defined(__AVR__)"));
  Serial.println(F("  #include <avr/pgmspace.h>"));
  Serial.println(F("#endif"));
  Serial.println();

  Serial.print(F("const uint8_t "));
  const char *p = filename;
  while (*p && *p != '.') {
    Serial.print(*p++);
  }
  Serial.println(F("[] PROGMEM = {"));

  uint8_t  b;
  uint8_t  col = 0;
  while (jpgFile.available()) {
    b = jpgFile.read();
    Serial.print(F("0x"));
    if (b < 16) Serial.print('0');
    Serial.print(b, HEX);
    Serial.print(',');

    if (++col >= 32) {
      col = 0;
      Serial.println();
    }
  }

  Serial.println(F("};"));
  jpgFile.close();
}
