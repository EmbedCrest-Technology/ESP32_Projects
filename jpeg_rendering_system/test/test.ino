/*
 * Author: EmbedCrest Technology Private Limited
 */
// -------------------------------------------------------
// ILI9341 GFX timing & demo routines
// -------------------------------------------------------

#include <SPI.h>
#include "Adafruit_GFX.h"
#include "Adafruit_ILI9341.h"

// -------------------------------------------------------
// TFT pin mapping & object
// -------------------------------------------------------
#define TFT_DC   17
#define TFT_CS   5
#define TFT_RST  4

static Adafruit_ILI9341 g_tft(TFT_CS, TFT_DC, TFT_RST);

// -------------------------------------------------------
// Notification handler (for benchmark logging)
// -------------------------------------------------------
static void logPhase(const char* label) {
  Serial.print("[GFX] ");
  Serial.println(label);
}

// -------------------------------------------------------
// Timing helpers
// -------------------------------------------------------
static unsigned long runFillScreenTest(uint16_t color);
static unsigned long runTextDemo();
static unsigned long runGradientRings();

// -------------------------------------------------------
// Setup
// -------------------------------------------------------
void setup() {
  Serial.begin(115200);
  while (!Serial) {
    ;
  }

  Serial.println();
  Serial.println("=== ILI9341 GFX Demo (Refactored) ===");

  g_tft.begin();
  g_tft.setRotation(1);
  g_tft.fillScreen(ILI9341_BLACK);

  // Basic banner
  g_tft.setTextSize(2);
  g_tft.setTextColor(ILI9341_WHITE, ILI9341_BLACK);
  g_tft.setCursor(10, 10);
  g_tft.println("ILI9341 Benchmark");
}

// -------------------------------------------------------
// Loop: run benchmarks repeatedly
// -------------------------------------------------------
void loop() {
  unsigned long t;

  logPhase("Fill screen (RED)");
  t = runFillScreenTest(ILI9341_RED);
  Serial.print("Fill RED:   ");
  Serial.print(t / 1000.0, 3);
  Serial.println(" ms");

  logPhase("Fill screen (GREEN)");
  t = runFillScreenTest(ILI9341_GREEN);
  Serial.print("Fill GREEN: ");
  Serial.print(t / 1000.0, 3);
  Serial.println(" ms");

  logPhase("Fill screen (BLUE)");
  t = runFillScreenTest(ILI9341_BLUE);
  Serial.print("Fill BLUE:  ");
  Serial.print(t / 1000.0, 3);
  Serial.println(" ms");

  logPhase("Text demo");
  t = runTextDemo();
  Serial.print("Text demo:  ");
  Serial.print(t / 1000.0, 3);
  Serial.println(" ms");

  logPhase("Gradient rings");
  t = runGradientRings();
  Serial.print("Rings demo: ");
  Serial.print(t / 1000.0, 3);
  Serial.println(" ms");

  Serial.println("-----------------------------");
  delay(2000);
}

// -------------------------------------------------------
// Fill screen test
// -------------------------------------------------------
static unsigned long runFillScreenTest(uint16_t color) {
  unsigned long start = micros();
  g_tft.fillScreen(color);
  yield();
  return micros() - start;
}

// -------------------------------------------------------
// Text rendering demo
// -------------------------------------------------------
static unsigned long runTextDemo() {
  unsigned long start = micros();

  g_tft.fillScreen(ILI9341_BLACK);

  g_tft.setCursor(0, 0);
  g_tft.setTextColor(ILI9341_WHITE);
  g_tft.setTextSize(1);
  g_tft.println("Hello from EmbedCrest!");
  g_tft.println("ILI9341 performance test");

  g_tft.setTextColor(ILI9341_YELLOW);
  g_tft.setTextSize(2);
  g_tft.println("1234.56");

  g_tft.setTextColor(ILI9341_RED);
  g_tft.setTextSize(3);
  g_tft.println("Graphics");

  g_tft.setTextColor(ILI9341_CYAN);
  g_tft.setTextSize(2);
  g_tft.println("ABCDEFGHIJKLMNOPQRSTUVWXYZ");
  g_tft.println("abcdefghijklmnopqrstuvwx");

  yield();
  return micros() - start;
}

// -------------------------------------------------------
// Radial gradient / nested rounded-rect demo
// -------------------------------------------------------
static unsigned long runGradientRings() {
  unsigned long start = micros();

  int16_t cx = g_tft.width()  / 2;
  int16_t cy = g_tft.height() / 2;

  g_tft.fillScreen(ILI9341_BLACK);

  // Draw shrinking rounded rectangles with varying green component
  for (int16_t size = min(g_tft.width(), g_tft.height()); size > 20; size -= 6) {
    int16_t halfSize = size / 2;
    uint8_t intensity = map(size, 20, min(g_tft.width(), g_tft.height()), 255, 0);
    uint16_t color = g_tft.color565(0, intensity, 0);

    g_tft.fillRoundRect(cx - halfSize,
                        cy - halfSize,
                        size,
                        size,
                        size / 8,
                        color);
    yield();
  }

  return micros() - start;
}
