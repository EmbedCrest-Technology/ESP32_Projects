 /* 
  * Author: EmbedCrest Technology Private Limited
  */
/////////////////////////////////////////////////////////////////
/*
  Dual-Core Benchmark Demo – Portenta H7
  - Core M7: Pi approximation benchmark
  - Core M4: Prime number benchmark
*/
/////////////////////////////////////////////////////////////////

#include "Arduino.h"
#include "RPC.h"
#include <Arduino_GFX_Library.h>

// -------------------------------------------------------
// Display configuration
// -------------------------------------------------------
Arduino_DataBus *gfxBus = new Arduino_HWSPI(D6, D7);
Arduino_GFX     *gfx    = new Arduino_ILI9341(gfxBus, DF_GFX_RST, 0, false);

// -------------------------------------------------------
// Benchmark configuration
// -------------------------------------------------------
static const int32_t PI_ITERATIONS        = 10000000;   // Leibniz series iterations
static const int32_t PRIME_SEARCH_LIMIT   = 300000;     // Upper bound for prime search

// -------------------------------------------------------
// Runtime metrics
// -------------------------------------------------------
static volatile uint32_t g_drawTimeCore0_ms = 0;
static volatile uint32_t g_drawTimeCore1_ms = 0;

static int g_prevProgressCore0 = 0;
static int g_prevProgressCore1 = 0;

static int g_primeCount = 0;

// -------------------------------------------------------
// Forward declarations
// -------------------------------------------------------
static void runCoreM7();
static void runCoreM4();

static String getActiveCoreName();
static double runPiBenchmark(int32_t iterations, int coreId);
static void   runPrimeBenchmark(int32_t limit, int coreId);
static void   refreshProgress(int index, int last, int coreId);
static void   renderProgressBar(
    uint16_t x,
    uint16_t y,
    uint16_t w,
    uint16_t h,
    uint8_t  percent,
    uint16_t frameColor,
    uint16_t barColor
);

// -------------------------------------------------------
// Notification handler
// setup()
// -------------------------------------------------------
void setup()
{
  gfx->begin();
  gfx->setRotation(2);

#ifdef CORE_CM7
  gfx->fillScreen(BLACK);

  // -------------------------------------------------------
  // Boot secondary core and run Pi benchmark on M7
  // -------------------------------------------------------
  bootM4();
  delay(1000);
  runCoreM7();
#endif

#ifdef CORE_CM4
  // Small delay to let M7 configure the display first
  delay(100);
  runCoreM4();
#endif
}

// -------------------------------------------------------
// loop()
// -------------------------------------------------------
void loop()
{
  // All work is done once in setup() per core
}

// -------------------------------------------------------
// Utility: readable core name
// -------------------------------------------------------
static String getActiveCoreName()
{
  if (HAL_GetCurrentCPUID() == CM7_CPUID)
  {
    return "M7";
  }
  else
  {
    return "M4";
  }
}

// -------------------------------------------------------
// Notification handler
// Core M7 path – Pi benchmark and top progress bar
// -------------------------------------------------------
static void runCoreM7()
{
  gfx->setTextColor(WHITE);
  gfx->setTextSize(2);
  gfx->setCursor(0, 10);
  gfx->println("Dual Core Test");
  gfx->println("Portenta H7");

  gfx->setTextSize(4);
  gfx->setCursor(gfx->width() - 60, 12);
  gfx->setTextColor(GREEN);
  gfx->print("H7");

  gfx->setTextColor(WHITE);
  gfx->setTextSize(2);
  gfx->setCursor(0, gfx->height() / 2 - 100);
  gfx->print("Core:");
  gfx->println(getActiveCoreName());

  gfx->setTextSize(1);
  gfx->print("Calculation of Pi (Leibniz)");

  // -------------------------------------------------------
  // Pi calculation + timing (exclude draw overhead)
// -------------------------------------------------------
  unsigned long tStart_us = micros();
  double piApprox          = runPiBenchmark(PI_ITERATIONS, 0);
  (void)piApprox; // result is not displayed, only used as workload

  unsigned long elapsed_ms =
      (micros() - tStart_us) / 1000UL - g_drawTimeCore0_ms;

  gfx->setTextColor(WHITE);
  gfx->setTextSize(2);
  gfx->setCursor(0, gfx->height() / 2 - 20);
  gfx->print("Elapsed Time:");
  gfx->print(elapsed_ms);
  gfx->println("ms");
}

// -------------------------------------------------------
// Notification handler
// Core M4 path – prime benchmark and bottom progress bar
// -------------------------------------------------------
static void runCoreM4()
{
  gfx->setTextColor(WHITE);
  gfx->setTextSize(2);
  gfx->setCursor(0, gfx->height() / 2 + 10);
  gfx->print("Core:");
  gfx->println(getActiveCoreName());

  gfx->setTextSize(1);
  gfx->print("Find Prime Numbers");

  unsigned long tStart_us = micros();
  runPrimeBenchmark(PRIME_SEARCH_LIMIT, 1);

  unsigned long elapsed_ms =
      (micros() - tStart_us) / 1000UL - g_drawTimeCore1_ms;

  gfx->setTextColor(WHITE);
  gfx->setTextSize(2);
  gfx->setCursor(0, gfx->height() / 2 + 100);
  gfx->print("Elapsed Time:");
  gfx->print(elapsed_ms);
  gfx->println("ms");
}

// -------------------------------------------------------
// Pi benchmark – Leibniz series
// -------------------------------------------------------
static double runPiBenchmark(int32_t iterations, int coreId)
{
  double sum  = 0.0;
  double term = 0.0;

  for (int32_t i = 0; i < iterations; ++i)
  {
    // Leibniz: pi = 4 * sum((-1)^i / (2i + 1))
    term = ((i & 1) ? -1.0 : 1.0) / (2.0 * i + 1.0);
    sum += term;

    refreshProgress(i, iterations - 1, coreId);
  }

  return 4.0 * sum;
}

// -------------------------------------------------------
// Prime benchmark – simple trial division
// -------------------------------------------------------
static void runPrimeBenchmark(int32_t limit, int coreId)
{
  for (int32_t i = 2; i < limit; ++i)
  {
    bool isPrime = true;

    for (int32_t j = 2; j <= i / 2; ++j)
    {
      if ((i % j) == 0)
      {
        isPrime = false;
        break;
      }
    }

    if (isPrime)
    {
      g_primeCount++;
    }

    refreshProgress(i, limit - 1, coreId);
  }
}

// -------------------------------------------------------
// Notification handler
// Progress update for each core
// -------------------------------------------------------
static void refreshProgress(int index, int last, int coreId)
{
  if (last <= 0) return;

  int percentage = (index * 100) / last;

  if (coreId == 0)
  {
    if (percentage != g_prevProgressCore0)
    {
      unsigned long drawStart_us = micros();

      renderProgressBar(
          20,
          gfx->height() / 2 - 60,
          gfx->width() - 50,
          20,
          (uint8_t)percentage,
          RED,
          GREEN
      );

      g_drawTimeCore0_ms += (micros() - drawStart_us) / 1000UL;
      g_prevProgressCore0 = percentage;
    }
  }
  else
  {
    if (percentage != g_prevProgressCore1)
    {
      unsigned long drawStart_us = micros();

      renderProgressBar(
          20,
          gfx->height() / 2 + 60,
          gfx->width() - 50,
          20,
          (uint8_t)percentage,
          RED,
          GREEN
      );

      g_drawTimeCore1_ms += (micros() - drawStart_us) / 1000UL;
      g_prevProgressCore1 = percentage;
    }
  }
}

// -------------------------------------------------------
// Progress bar renderer
// -------------------------------------------------------
static void renderProgressBar(
    uint16_t x,
    uint16_t y,
    uint16_t w,
    uint16_t h,
    uint8_t  percent,
    uint16_t frameColor,
    uint16_t barColor
)
{
  const uint8_t margin    = 2;
  const uint16_t innerH   = h - 2 * margin;
  const uint16_t innerW   = w - 2 * margin;
  const uint16_t fillW    = (uint16_t)(innerW * (percent / 100.0f));

  gfx->drawRoundRect(x, y, w, h, 3, frameColor);
  gfx->fillRect(x + margin, y + margin, fillW, innerH, barColor);
}
