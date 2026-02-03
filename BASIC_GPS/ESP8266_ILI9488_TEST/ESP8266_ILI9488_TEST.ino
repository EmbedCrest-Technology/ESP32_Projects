/* 
 *TFT_eSPI Benchmark Demo
 *Based on original Adafruit graphicstest example.
 *
 * Author: EmbedCrest Technology Private Limited
 */


#include <SPI.h>
#include <TFT_eSPI.h>

// -------------------------------------------------------
// Display object
// -------------------------------------------------------
TFT_eSPI benchTft = TFT_eSPI();

unsigned long g_totalTime = 0;
unsigned long g_startMicros = 0;

// -------------------------------------------------------
// Forward declarations
// -------------------------------------------------------
unsigned long benchFillScreen();
unsigned long benchText();
unsigned long benchLines(uint16_t c);
unsigned long benchFastLines(uint16_t c1, uint16_t c2);
unsigned long benchRects(uint16_t c);
unsigned long benchFilledRects(uint16_t c1, uint16_t c2);
unsigned long benchFilledCircles(uint8_t r, uint16_t c);
unsigned long benchCircles(uint8_t r, uint16_t c);
unsigned long benchTriangles();
unsigned long benchFilledTriangles();
unsigned long benchRoundRects();
unsigned long benchFilledRoundRects();

// -------------------------------------------------------
// Notification handler
// setup()
// -------------------------------------------------------
void setup()
{
  Serial.begin(115200);
  while (!Serial) { }

  Serial.println();
  Serial.println();
  Serial.println("TFT_eSPI library test!");

  benchTft.init();

  g_startMicros = micros();
  benchTft.fillScreen(TFT_BLACK);

  yield(); Serial.println(F("Benchmark                Time (microseconds)"));

  yield(); Serial.print(F("Screen fill              "));
  yield(); Serial.println(benchFillScreen());

  yield(); Serial.print(F("Text                     "));
  yield(); Serial.println(benchText());

  yield(); Serial.print(F("Lines                    "));
  yield(); Serial.println(benchLines(TFT_CYAN));

  yield(); Serial.print(F("Horiz/Vert Lines         "));
  yield(); Serial.println(benchFastLines(TFT_RED, TFT_BLUE));

  yield(); Serial.print(F("Rectangles (outline)     "));
  yield(); Serial.println(benchRects(TFT_GREEN));

  yield(); Serial.print(F("Rectangles (filled)      "));
  yield(); Serial.println(benchFilledRects(TFT_YELLOW, TFT_MAGENTA));

  yield(); Serial.print(F("Circles (filled)         "));
  yield(); Serial.println(benchFilledCircles(10, TFT_MAGENTA));

  yield(); Serial.print(F("Circles (outline)        "));
  yield(); Serial.println(benchCircles(10, TFT_WHITE));

  yield(); Serial.print(F("Triangles (outline)      "));
  yield(); Serial.println(benchTriangles());

  yield(); Serial.print(F("Triangles (filled)       "));
  yield(); Serial.println(benchFilledTriangles());

  yield(); Serial.print(F("Rounded rects (outline)  "));
  yield(); Serial.println(benchRoundRects());

  yield(); Serial.print(F("Rounded rects (filled)   "));
  yield(); Serial.println(benchFilledRoundRects());

  yield(); Serial.println(F("Done!"));
}

// -------------------------------------------------------
// loop() – rotate and show text test
// -------------------------------------------------------
void loop()
{
  for (uint8_t rot = 0; rot < 4; ++rot)
  {
    benchTft.setRotation(rot);
    benchText();
    delay(2000);
  }
}

// -------------------------------------------------------
// Individual benchmarks
// -------------------------------------------------------
unsigned long benchFillScreen()
{
  unsigned long start = micros();
  benchTft.fillScreen(TFT_BLACK);
  benchTft.fillScreen(TFT_RED);
  benchTft.fillScreen(TFT_GREEN);
  benchTft.fillScreen(TFT_BLUE);
  benchTft.fillScreen(TFT_BLACK);
  return micros() - start;
}

// -------------------------------------------------------
// Notification handler
// -------------------------------------------------------
unsigned long benchText()
{
  benchTft.fillScreen(TFT_BLACK);

  unsigned long start = micros();
  benchTft.setCursor(0, 0);
  benchTft.setTextColor(TFT_WHITE);
  benchTft.setTextSize(1);
  benchTft.println("Hello World!");

  benchTft.setTextColor(TFT_YELLOW);
  benchTft.setTextSize(2);
  benchTft.println(1234.56);

  benchTft.setTextColor(TFT_RED);
  benchTft.setTextSize(3);
  benchTft.println(0xDEADBEEF, HEX);
  benchTft.println();

  benchTft.setTextColor(TFT_GREEN);
  benchTft.setTextSize(5);
  benchTft.println("Groop");

  benchTft.setTextSize(2);
  benchTft.println("I implore thee,");

  benchTft.setTextSize(1);
  benchTft.println("my foonting turlingdromes.");
  benchTft.println("And hooptiously drangle me");
  benchTft.println("with crinkly bindlewurdles,");
  benchTft.println("Or I will rend thee");
  benchTft.println("in the gobberwarts");
  benchTft.println("with my blurglecruncheon,");
  benchTft.println("see if I don't!");

  return micros() - start;
}

unsigned long benchLines(uint16_t color)
{
  unsigned long start, t;
  int x1, y1, x2, y2;
  int w = benchTft.width();
  int h = benchTft.height();

  benchTft.fillScreen(TFT_BLACK);

  x1 = y1 = 0;
  y2 = h - 1;
  start = micros();
  for (x2 = 0; x2 < w; x2 += 6) benchTft.drawLine(x1, y1, x2, y2, color);
  x2 = w - 1;
  for (y2 = 0; y2 < h; y2 += 6) benchTft.drawLine(x1, y1, x2, y2, color);
  t = micros() - start;
  yield();
  benchTft.fillScreen(TFT_BLACK);

  x1 = w - 1;
  y1 = 0;
  y2 = h - 1;
  start = micros();
  for (x2 = 0; x2 < w; x2 += 6) benchTft.drawLine(x1, y1, x2, y2, color);
  x2 = 0;
  for (y2 = 0; y2 < h; y2 += 6) benchTft.drawLine(x1, y1, x2, y2, color);
  t += micros() - start;
  yield();
  benchTft.fillScreen(TFT_BLACK);

  x1 = 0;
  y1 = h - 1;
  y2 = 0;
  start = micros();
  for (x2 = 0; x2 < w; x2 += 6) benchTft.drawLine(x1, y1, x2, y2, color);
  x2 = w - 1;
  for (y2 = 0; y2 < h; y2 += 6) benchTft.drawLine(x1, y1, x2, y2, color);
  t += micros() - start;
  yield();
  benchTft.fillScreen(TFT_BLACK);

  x1 = w - 1;
  y1 = h - 1;
  y2 = 0;
  start = micros();
  for (x2 = 0; x2 < w; x2 += 6) benchTft.drawLine(x1, y1, x2, y2, color);
  x2 = 0;
  for (y2 = 0; y2 < h; y2 += 6) benchTft.drawLine(x1, y1, x2, y2, color);
  yield();

  return t + (micros() - start);
}

unsigned long benchFastLines(uint16_t color1, uint16_t color2)
{
  unsigned long start;
  int w = benchTft.width();
  int h = benchTft.height();

  benchTft.fillScreen(TFT_BLACK);
  start = micros();
  for (int y = 0; y < h; y += 5) benchTft.drawFastHLine(0, y, w, color1);
  for (int x = 0; x < w; x += 5) benchTft.drawFastVLine(x, 0, h, color2);
  return micros() - start;
}

unsigned long benchRects(uint16_t color)
{
  unsigned long start;
  int cx = benchTft.width() / 2;
  int cy = benchTft.height() / 2;
  int n = min(benchTft.width(), benchTft.height());

  benchTft.fillScreen(TFT_BLACK);
  start = micros();
  for (int i = 2; i < n; i += 6)
  {
    int i2 = i / 2;
    benchTft.drawRect(cx - i2, cy - i2, i, i, color);
  }
  return micros() - start;
}

unsigned long benchFilledRects(uint16_t color1, uint16_t color2)
{
  unsigned long start, total = 0;
  int cx = benchTft.width() / 2 - 1;
  int cy = benchTft.height() / 2 - 1;
  int n = min(benchTft.width(), benchTft.height());

  benchTft.fillScreen(TFT_BLACK);
  for (int i = n - 1; i > 0; i -= 6)
  {
    int i2 = i / 2;
    start = micros();
    benchTft.fillRect(cx - i2, cy - i2, i, i, color1);
    total += micros() - start;

    benchTft.drawRect(cx - i2, cy - i2, i, i, color2);
  }
  return total;
}

unsigned long benchFilledCircles(uint8_t radius, uint16_t color)
{
  unsigned long start;
  int w = benchTft.width();
  int h = benchTft.height();
  int r2 = radius * 2;

  benchTft.fillScreen(TFT_BLACK);
  start = micros();
  for (int x = radius; x < w; x += r2)
  {
    for (int y = radius; y < h; y += r2)
    {
      benchTft.fillCircle(x, y, radius, color);
    }
  }
  return micros() - start;
}

unsigned long benchCircles(uint8_t radius, uint16_t color)
{
  unsigned long start;
  int r2 = radius * 2;
  int w = benchTft.width() + radius;
  int h = benchTft.height() + radius;

  start = micros();
  for (int x = 0; x < w; x += r2)
  {
    for (int y = 0; y < h; y += r2)
    {
      benchTft.drawCircle(x, y, radius, color);
    }
  }
  return micros() - start;
}

unsigned long benchTriangles()
{
  unsigned long start;
  int cx = benchTft.width() / 2 - 1;
  int cy = benchTft.height() / 2 - 1;

  benchTft.fillScreen(TFT_BLACK);
  int n = min(cx, cy);

  start = micros();
  for (int i = 0; i < n; i += 5)
  {
    benchTft.drawTriangle(
        cx,     cy - i,
        cx - i, cy + i,
        cx + i, cy + i,
        benchTft.color565(0, 0, i));
  }
  return micros() - start;
}

unsigned long benchFilledTriangles()
{
  unsigned long total = 0;
  int cx = benchTft.width() / 2 - 1;
  int cy = benchTft.height() / 2 - 1;

  benchTft.fillScreen(TFT_BLACK);
  for (int i = min(cx, cy); i > 10; i -= 5)
  {
    unsigned long start = micros();
    benchTft.fillTriangle(
        cx,     cy - i,
        cx - i, cy + i,
        cx + i, cy + i,
        benchTft.color565(0, i, i));
    total += micros() - start;

    benchTft.drawTriangle(
        cx,     cy - i,
        cx - i, cy + i,
        cx + i, cy + i,
        benchTft.color565(i, i, 0));
  }
  return total;
}

unsigned long benchRoundRects()
{
  unsigned long start;
  int cx = benchTft.width() / 2 - 1;
  int cy = benchTft.height() / 2 - 1;
  int w  = min(benchTft.width(), benchTft.height());

  benchTft.fillScreen(TFT_BLACK);
  start = micros();
  for (int i = 0; i < w; i += 6)
  {
    int i2 = i / 2;
    benchTft.drawRoundRect(
        cx - i2, cy - i2,
        i, i,
        i / 8,
        benchTft.color565(i, 0, 0));
  }
  return micros() - start;
}

unsigned long benchFilledRoundRects()
{
  unsigned long start;
  int cx = benchTft.width() / 2 - 1;
  int cy = benchTft.height() / 2 - 1;

  benchTft.fillScreen(TFT_BLACK);
  start = micros();
  for (int i = min(benchTft.width(), benchTft.height()); i > 20; i -= 6)
  {
    int i2 = i / 2;
    benchTft.fillRoundRect(
        cx - i2, cy - i2,
        i, i,
        i / 8,
        benchTft.color565(0, i, 0));
    yield();
  }
  return micros() - start;
}
