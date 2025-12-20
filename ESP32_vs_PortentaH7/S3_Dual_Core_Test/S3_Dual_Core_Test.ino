/* 
 * Author: EmbedCrest Technology Private Limited
 */
/////////////////////////////////////////////////////////////////
/*
  Dual-Core Benchmark Demo – ESP32-S3 
  - Core 0: Pi approximation (Leibniz)
  - Core 1: Prime number search
*/
/////////////////////////////////////////////////////////////////

#include <esp_task_wdt.h>
#include <Arduino_GFX_Library.h>

// -------------------------------------------------------
// Display configuration
// -------------------------------------------------------
Arduino_DataBus *gfxBus = new Arduino_HWSPI(7, 10);
Arduino_GFX     *gfx    = new Arduino_ILI9341(gfxBus, DF_GFX_RST, 0, false);

// -------------------------------------------------------
// Benchmark parameters
// -------------------------------------------------------
static const int32_t PI_ITERATIONS      = 10000000;  // Pi calculation iterations
static const int32_t PRIME_MAX_LIMIT    = 300000;    // Max number to test for primality

// -------------------------------------------------------
// Runtime statistics
// -------------------------------------------------------
static volatile uint32_t g_drawTimeCore0_ms = 0;
static volatile uint32_t g_drawTimeCore1_ms = 0;

static int g_prevProgressCore0 = 0;
static int g_prevProgressCore1 = 0;

static int g_primeCounter      = 0;

// -------------------------------------------------------
// Forward declarations
// -------------------------------------------------------
static void core0Task(void *pvParameters);
static void core1Task(void *pvParameters);

static double executePiWorkload(int32_t iterations, int coreId);
static void   executePrimeWorkload(int32_t limit, int coreId);

static void   updateProgressBar(int index, int lastIndex, int coreId);
static void   drawProgressBar(
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
  gfx->fillScreen(BLACK);
  gfx->setRotation(2);

  // Title area
  gfx->setTextColor(WHITE);
  gfx->setTextSize(2);
  gfx->setCursor(0, 10);
  gfx->println("Dual Core Test");
  gfx->println("ESP32-S3");

  gfx->setTextSize(4);
  gfx->setCursor(gfx->width() - 60, 12);
  gfx->setTextColor(GREEN);
  gfx->print("S3");

  // -------------------------------------------------------
  // Spawn workload tasks on each core
  // -------------------------------------------------------
  xTaskCreatePinnedToCore(core0Task, "Core0Task", 10000, nullptr, 1, nullptr, 0);
  delay(100);
  xTaskCreatePinnedToCore(core1Task, "Core1Task", 10000, nullptr, 1, nullptr, 1);
}

// -------------------------------------------------------
// loop()
// -------------------------------------------------------
void loop()
{
  // All work is done in RTOS tasks
}

// -------------------------------------------------------
// Notification handler
// Core 0 task – Pi benchmark (upper half)
// -------------------------------------------------------
static void core0Task(void *pvParameters)
{
  // Long timeout, no panic
  esp_task_wdt_init(600, false);

  // Header for core 0 region
  gfx->setTextColor(WHITE);
  gfx->setTextSize(2);
  gfx->setCursor(0, gfx->height() / 2 - 100);
  gfx->print("Core:");
  gfx->println(xPortGetCoreID());

  gfx->setTextSize(1);
  gfx->print("Calculation of Pi (Leibniz)");

  // Run Pi workload and measure execution time (minus draw time)
  unsigned long tStart = millis();
  double piApprox = executePiWorkload(PI_ITERATIONS, xPortGetCoreID());
  (void)piApprox;  // value not shown, only used for workload

  unsigned long elapsed_ms = millis() - tStart - g_drawTimeCore0_ms;

  // Show elapsed time for Pi calculation
  gfx->setTextColor(WHITE);
  gfx->setTextSize(2);
  gfx->setCursor(0, gfx->height() / 2 - 20);
  gfx->print("Elapsed Time:");
  gfx->print(elapsed_ms);
  gfx->println("ms");

  vTaskDelete(nullptr);
}

// -------------------------------------------------------
// Notification handler
// Core 1 task – prime benchmark (lower half)
// -------------------------------------------------------
static void core1Task(void *pvParameters)
{
  // Long timeout, no panic
  esp_task_wdt_init(600, false);

  // Header for core 1 region
  gfx->setTextColor(WHITE);
  gfx->setTextSize(2);
  gfx->setCursor(0, gfx->height() / 2 + 10);
  gfx->print("Core:");
  gfx->println(xPortGetCoreID());

  gfx->setTextSize(1);
  gfx->print("Find Prime Numbers");

  // Run prime search workload and measure execution time (minus draw time)
  unsigned long tStart = millis();
  executePrimeWorkload(PRIME_MAX_LIMIT, 1);  // keep original behavior: coreId=1

  unsigned long elapsed_ms = millis() - tStart - g_drawTimeCore1_ms;

  // Show elapsed time for prime search
  gfx->setTextColor(WHITE);
  gfx->setTextSize(2);
  gfx->setCursor(0, gfx->height() / 2 + 100);
  gfx->print("Elapsed Time:");
  gfx->print(elapsed_ms);
  gfx->println("ms");

  vTaskDelete(nullptr);
}

// -------------------------------------------------------
// Pi workload – Leibniz series implementation
// -------------------------------------------------------
static double executePiWorkload(int32_t iterations, int coreId)
{
  double sum  = 0.0;
  double term = 0.0;

  for (int32_t i = 0; i < iterations; ++i)
  {
    // (+1, -1, +1, -1, ...)
    term = ((i & 1) ? -1.0 : 1.0) / (2.0 * i + 1.0);
    sum += term;

    updateProgressBar(i, iterations - 1, coreId);
  }

  return 4.0 * sum;
}

// -------------------------------------------------------
// Prime workload – simple trial division
// -------------------------------------------------------
static void executePrimeWorkload(int32_t limit, int coreId)
{
  for (int32_t candidate = 2; candidate < limit; ++candidate)
  {
    bool isPrime = true;

    for (int32_t div = 2; div <= candidate / 2; ++div)
    {
      if (candidate % div == 0)
      {
        isPrime = false;
        break;
      }
    }

    if (isPrime)
    {
      g_primeCounter++;
    }

    updateProgressBar(candidate, limit - 1, coreId);
  }
}

// -------------------------------------------------------
// Notification handler
// Progress update routing for each core
// -------------------------------------------------------
static void updateProgressBar(int index, int lastIndex, int coreId)
{
  if (lastIndex <= 0) return;

  int percent = (index * 100) / lastIndex;

  if (coreId == 0)
  {
    if (percent != g_prevProgressCore0)
    {
      unsigned long drawStart = millis();

      drawProgressBar(
          20,
          gfx->height() / 2 - 60,
          gfx->width() - 50,
          20,
          (uint8_t)percent,
          RED,
          GREEN
      );

      g_drawTimeCore0_ms += millis() - drawStart;
      g_prevProgressCore0 = percent;
    }
  }
  else
  {
    if (percent != g_prevProgressCore1)
    {
      unsigned long drawStart = millis();

      drawProgressBar(
          20,
          gfx->height() / 2 + 60,
          gfx->width() - 50,
          20,
          (uint8_t)percent,
          RED,
          GREEN
      );

      g_drawTimeCore1_ms += millis() - drawStart;
      g_prevProgressCore1 = percent;
    }
  }
}

// -------------------------------------------------------
// Progress bar drawing primitive
// -------------------------------------------------------
static void drawProgressBar(
    uint16_t x,
    uint16_t y,
    uint16_t w,
    uint16_t h,
    uint8_t  percent,
    uint16_t frameColor,
    uint16_t barColor
)
{
  const uint8_t  margin   = 2;
  const uint16_t innerH   = h - 2 * margin;
  const uint16_t innerW   = w - 2 * margin;
  const uint16_t filledW  = (uint16_t)(innerW * (percent / 100.0f));

  gfx->drawRoundRect(x, y, w, h, 3, frameColor);
  gfx->fillRect(x + margin, y + margin, filledW, innerH, barColor);
}
