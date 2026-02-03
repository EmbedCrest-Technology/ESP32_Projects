/* 
 * Author: EmbedCrest Technology Private Limited
 */
// -------------------------------------------------------
// SPIFFS + JPEGDecoder + ILI9341 TFT integration
// -------------------------------------------------------

#include "SPIFFS.h"
#include <SPI.h>
#include <JPEGDecoder.h>
#include "Adafruit_GFX.h"
#include "Adafruit_ILI9341.h"

// -------------------------------------------------------
// TFT pin mapping & global objects
// -------------------------------------------------------
#define TFT_DC   17
#define TFT_CS   5
#define TFT_RST  4

static Adafruit_ILI9341 g_tft(TFT_CS, TFT_DC, TFT_RST);
static bool             g_fsInitialized = false;

// JPEG file to process from SPIFFS
static const char* kJpegPath = "/image.jpg";

// -------------------------------------------------------
// Notification handler
// -------------------------------------------------------
// Included to match your requested comment style. This sketch
// does not use BLE, but you can reuse this section for any
// status/notification logging you may add later.
static void logNotification(const char* msg) {
  Serial.print("[Note] ");
  Serial.println(msg);
}

// -------------------------------------------------------
// SPIFFS initialization & TFT setup
// -------------------------------------------------------
static void initFilesystem() {
  if (!SPIFFS.begin()) {
    Serial.println("Failed to mount SPIFFS. Aborting.");
    while (true) {
      yield();
    }
  }
  g_fsInitialized = true;
  Serial.println("SPIFFS mounted successfully.");
}

static void initDisplay() {
  g_tft.begin();
  g_tft.fillScreen(ILI9341_BLACK);
  g_tft.setRotation(1);
  g_tft.setTextColor(ILI9341_WHITE, ILI9341_BLACK);
  g_tft.setTextSize(2);
  g_tft.setCursor(10, 10);
  g_tft.println("JPEG SPIFFS Demo");
}

// -------------------------------------------------------
// JPEG -> Hex dump (C array style) from SPIFFS
// -------------------------------------------------------
static void emitJpegAsHexArray(const char* filePath, const char* symbolName) {
  if (!g_fsInitialized) {
    Serial.println("Filesystem not initialized, cannot dump JPEG.");
    return;
  }

  File jpgFile = SPIFFS.open(filePath, "r");
  if (!jpgFile) {
    Serial.print("Unable to open JPEG file: ");
    Serial.println(filePath);
    return;
  }

  Serial.println();
  Serial.print("const uint8_t ");
  Serial.print(symbolName);
  Serial.println("[] PROGMEM = {");

  uint32_t lineCount = 0;
  uint8_t  byteVal   = 0;

  while (jpgFile.available()) {
    byteVal = jpgFile.read();

    // Print value in 0xHH format
    if (lineCount == 0) {
      Serial.print("  ");
    }

    Serial.print("0x");
    if (byteVal < 0x10) {
      Serial.print('0');
    }
    Serial.print(byteVal, HEX);
    Serial.print(',');

    lineCount++;
    if (lineCount >= 32) {
      Serial.println();
      lineCount = 0;
    }
    yield();
  }

  if (lineCount != 0) {
    Serial.println();
  }
  Serial.println("};");
  Serial.println();

  jpgFile.close();
  Serial.println("JPEG hex array export complete.");
}

// -------------------------------------------------------
// JPEG rendering to TFT using JPEGDecoder
// -------------------------------------------------------
static void drawJpegFromSPIFFS(const char* filename, int16_t x, int16_t y) {
  if (!g_fsInitialized) {
    Serial.println("Filesystem not initialized, cannot draw JPEG.");
    return;
  }

  // Decode JPEG from SPIFFS
  if (JpegDec.decodeFsFile(filename) != 1) {
    Serial.print("JPEGDecoder failed for file: ");
    Serial.println(filename);
    return;
  }

  uint16_t imgWidth  = JpegDec.width;
  uint16_t imgHeight = JpegDec.height;

  Serial.print("Decoded JPEG: ");
  Serial.print(imgWidth);
  Serial.print("x");
  Serial.println(imgHeight);

  // Draw line-by-line to TFT
  while (JpegDec.read()) {
    uint16_t* pImg = JpegDec.pImage;
    int16_t   mcuX = JpegDec.MCUx * JpegDec.MCUWidth + x;
    int16_t   mcuY = JpegDec.MCUy * JpegDec.MCUHeight + y;
    int16_t   mcuW = JpegDec.MCUWidth;
    int16_t   mcuH = JpegDec.MCUHeight;

    if (mcuX + mcuW > g_tft.width()) {
      mcuW = g_tft.width() - mcuX;
    }
    if (mcuY + mcuH > g_tft.height()) {
      mcuH = g_tft.height() - mcuY;
    }

    if (mcuW <= 0 || mcuH <= 0) {
      continue;
    }

    g_tft.startWrite();
    g_tft.setAddrWindow(mcuX, mcuY, mcuW, mcuH);
    for (int16_t row = 0; row < mcuH; ++row) {
      for (int16_t col = 0; col < mcuW; ++col) {
        uint16_t pixel = *pImg++;
        g_tft.writePixel(pixel);
      }
    }
    g_tft.endWrite();
    yield();
  }
}

// -------------------------------------------------------
// Setup & Loop
// -------------------------------------------------------
void setup() {
  Serial.begin(115200);
  while (!Serial) {
    ;
  }

  Serial.println();
  Serial.println("=== JPEG + TFT SPIFFS Demo (Refactored) ===");

  initFilesystem();
  initDisplay();

  logNotification("Starting JPEG rendering from SPIFFS.");
  drawJpegFromSPIFFS(kJpegPath, 0, 40);

  logNotification("Exporting JPEG as C-style hex array.");
  emitJpegAsHexArray(kJpegPath, "embedded_jpeg_data");
}

void loop() {
  // Nothing dynamic here; left for extension/testing
  delay(1000);
}
