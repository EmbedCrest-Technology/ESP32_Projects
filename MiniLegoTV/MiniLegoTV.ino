 /* 
  * Mini Lego TV
  *
  * Author: EmbedCrest Technology Private Limited
  *
  */

// -------------------------------------------------------
// Includes & configuration
// -------------------------------------------------------

#include "config.h"

#include <WiFi.h>
#include <FS.h>
#include <SD.h>
#include <Preferences.h>

#include <Arduino_GFX_Library.h>

#include "esp32_audio_task.h"
#include "mjpeg_decode_draw_task.h"

#include <CST816S.h>

// -------------------------------------------------------
// Application constants
// -------------------------------------------------------

#define FPS                     30
#define MJPEG_BUFFER_SIZE       (288 * 240 * 2 / 8)

#define AUDIOASSIGNCORE         1
#define DECODEASSIGNCORE        0
#define DRAWASSIGNCORE          1

#define APP_NAME                "video_player"
#define K_VIDEO_INDEX           "video_index"

#define BASE_PATH               "/Videos/"
#define AAC_FILENAME            "/44100.aac"
#define MJPEG_FILENAME          "/288_30fps.mjpeg"

#define VIDEO_COUNT             20

// -------------------------------------------------------
// Display & touch objects
// -------------------------------------------------------

Arduino_DataBus *bus = new Arduino_ESP32SPI(DC, CS, SCK, MOSI, MISO, VSPI);
Arduino_GFX     *gfx = new Arduino_ST7789(
    bus,
    RST,
    1,
    true,
    ST7789_TFTWIDTH,
    ST7789_TFTHEIGHT,
    0, 20, 0, 20
);

CST816S touch(TOUCH_SDA, TOUCH_SCL, TOUCH_RST, TOUCH_IRQ);

// -------------------------------------------------------
// State variables
// -------------------------------------------------------

Preferences prefs;

static unsigned int currentChannel = 1;

static int           frameIndex      = 0;
static int           droppedFrames   = 0;
static unsigned long tStart          = 0;
static unsigned long tNow            = 0;
static unsigned long tNextFrame      = 0;

// -------------------------------------------------------
// Video draw callback (JPEGDEC -> TFT)
// -------------------------------------------------------

static int videoDrawCallback(JPEGDRAW *pDraw)
{
    unsigned long t0 = millis();

    // Push MCU block to screen
    gfx->draw16bitRGBBitmap(
        pDraw->x,
        pDraw->y,
        pDraw->pPixels,
        pDraw->iWidth,
        pDraw->iHeight
    );

    total_show_video_ms += millis() - t0;
    return 1;
}

// -------------------------------------------------------
// Forward declarations
// -------------------------------------------------------

static void touchMonitorTask(void *parameter);
static void playChannel(unsigned int channel);
static void switchChannel(int delta);

// -------------------------------------------------------
// setup()
// -------------------------------------------------------

void setup()
{
    // Watchdog & radio off for deterministic playback
    disableCore0WDT();
    WiFi.mode(WIFI_OFF);

    Serial.begin(115200);

    // ---------------------------------------------------
    // Display init
    // ---------------------------------------------------
    gfx->begin(80000000);
    gfx->fillScreen(BLACK);

    pinMode(BLK, OUTPUT);
    digitalWrite(BLK, HIGH);

    // Start touch monitor on a separate task
    xTaskCreate(
        touchMonitorTask,
        "touchTask",
        2000,
        NULL,
        1,
        NULL
    );

    // ---------------------------------------------------
    // Audio (I2S) init
    // ---------------------------------------------------
    Serial.println("Init I2S");

    esp_err_t audioInitResult =
        i2s_init(I2S_NUM_0, 44100, I2S_MCLK, I2S_SCLK, I2S_LRCLK, I2S_DOUT, I2S_DIN);

    if (audioInitResult != ESP_OK)
    {
        Serial.printf("i2s_init failed: %d\n", audioInitResult);
        gfx->println("i2s_init failed");
        return;
    }

    i2s_zero_dma_buffer(I2S_NUM_0);

    // ---------------------------------------------------
    // SD card / filesystem init
    // ---------------------------------------------------
    Serial.println("Init FS");

    SPIClass spiBus(HSPI);
    spiBus.begin(SD_SCK, SD_MISO, SD_MOSI, SD_CS);

    if (!SD.begin(SD_CS, spiBus, 80000000))
    {
        Serial.println("ERROR: File system mount failed!");
        gfx->println("ERROR: File system mount failed!");
        return;
    }

    // ---------------------------------------------------
    // Load last channel from NVS
    // ---------------------------------------------------
    prefs.begin(APP_NAME, false);
    currentChannel = prefs.getUInt(K_VIDEO_INDEX, 1);

    Serial.printf("videoIndex: %u\n", currentChannel);

    gfx->setCursor(20, 20);
    gfx->setTextColor(GREEN);
    gfx->setTextSize(6, 6, 0);
    gfx->printf("CH %u", currentChannel);

    delay(1000);

    // Start playback
    playChannel(currentChannel);
}

// -------------------------------------------------------
// loop() – nothing, everything is task driven
// -------------------------------------------------------

void loop()
{
    // Main work is handled by tasks and blocking playback
}

// -------------------------------------------------------
// Notification handler – Touch / Gesture task
// -------------------------------------------------------

static void touchMonitorTask(void *parameter)
{
    touch.begin();

    while (true)
    {
        if (touch.available())
        {
            Serial.printf("x: %d, y: %d\n", touch.data.x, touch.data.y);

            switch (touch.data.gestureID)
            {
                case SWIPE_LEFT:
                    Serial.println("SWIPE_LEFT");
                    break;

                case SWIPE_RIGHT:
                    Serial.println("SWIPE_RIGHT");
                    break;

                case SWIPE_UP:
                    Serial.println("SWIPE_UP (RIGHT)");
                    // previous channel
                    switchChannel(-1);
                    break;

                case SWIPE_DOWN:
                    Serial.println("SWIPE_DOWN (LEFT)");
                    // next channel
                    switchChannel(1);
                    break;

                case SINGLE_CLICK:
                    Serial.println("SINGLE_CLICK");
                    break;

                case DOUBLE_CLICK:
                    Serial.println("DOUBLE_CLICK");
                    break;

                case LONG_PRESS:
                    Serial.println("LONG_PRESS");
                    break;

                default:
                    break;
            }
        }

        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}

// -------------------------------------------------------
// Helper – open audio & video files for a channel
// -------------------------------------------------------

static bool openMediaFiles(
    unsigned int channel,
    File &audioFile,
    File &videoFile
)
{
    char audioPath[40];
    char videoPath[40];

    snprintf(audioPath, sizeof(audioPath), "%s%u%s", BASE_PATH, channel, AAC_FILENAME);
    snprintf(videoPath, sizeof(videoPath), "%s%u%s", BASE_PATH, channel, MJPEG_FILENAME);

    audioFile = SD.open(audioPath);
    if (!audioFile || audioFile.isDirectory())
    {
        Serial.printf("ERROR: Failed to open %s\n", audioPath);
        gfx->printf("ERROR: Failed to open %s\n", audioPath);
        return false;
    }

    videoFile = SD.open(videoPath);
    if (!videoFile || videoFile.isDirectory())
    {
        Serial.printf("ERROR: Failed to open %s\n", videoPath);
        gfx->printf("ERROR: Failed to open %s\n", videoPath);
        audioFile.close();
        return false;
    }

    return true;
}

// -------------------------------------------------------
// Video + audio player for a given channel
// -------------------------------------------------------

static void playChannel(unsigned int channel)
{
    File audioFile;
    File videoFile;

    // Try to open both media streams
    if (!openMediaFiles(channel, audioFile, videoFile))
    {
        return;
    }

    Serial.println("Init video");

    // Configure MJPEG decoder / renderer
    mjpeg_setup(
        &videoFile,
        MJPEG_BUFFER_SIZE,
        videoDrawCallback,
        false,
        DECODEASSIGNCORE,
        DRAWASSIGNCORE
    );

    // ---------------------------------------------------
    // Start audio player (AAC)
    // ---------------------------------------------------
    Serial.println("Start play audio task");

    BaseType_t audioTaskResult = aac_player_task_start(&audioFile, AUDIOASSIGNCORE);
    if (audioTaskResult != pdPASS)
    {
        Serial.printf("Audio player task start failed: %d\n", audioTaskResult);
        gfx->printf("Audio task failed: %d\n", audioTaskResult);
    }

    // ---------------------------------------------------
    // MJPEG playback loop with frame scheduling
    // ---------------------------------------------------
    Serial.println("Start play video");

    frameIndex    = 0;
    droppedFrames = 0;

    tStart     = millis();
    tNow       = tStart;
    // First frame uses half interval – keep original timing behavior
    tNextFrame = tStart + (++frameIndex * 1000 / FPS / 2);

    while (videoFile.available() && mjpeg_read_frame())
    {
        // account for video read time
        total_read_video_ms += millis() - tNow;
        tNow = millis();

        // Decide whether to show or skip the decoded frame
        if (millis() < tNextFrame)
        {
            // Draw current frame
            mjpeg_draw_frame();

            total_decode_video_ms += millis() - tNow;
            tNow = millis();
        }
        else
        {
            ++droppedFrames;
            Serial.println("Skip frame");
        }

        // Wait until the exact frame deadline
        while (millis() < tNextFrame)
        {
            vTaskDelay(pdMS_TO_TICKS(1));
        }

        tNow       = millis();
        tNextFrame = tStart + (++frameIndex * 1000 / FPS);
    }

    // ---------------------------------------------------
    // Cleanup and chain to next channel
    // ---------------------------------------------------
    int elapsed = millis() - tStart;
    int totalFrames = frameIndex - 1;

    Serial.println("AV end");
    Serial.printf("frames: %d, dropped: %d, time: %d ms\n",
                  totalFrames, droppedFrames, elapsed);

    videoFile.close();
    audioFile.close();

    // Auto advance to next channel when current finishes
    switchChannel(1);
}

// -------------------------------------------------------
// Notification handler – Channel management
// -------------------------------------------------------

static void switchChannel(int delta)
{
    // Update index with wrap-around
    int nextChannel = (int)currentChannel + delta;

    if (nextChannel <= 0)
    {
        nextChannel = VIDEO_COUNT;
    }
    else if (nextChannel > VIDEO_COUNT)
    {
        nextChannel = 1;
    }

    currentChannel = (unsigned int)nextChannel;

    Serial.printf("video_idx : %u\n", currentChannel);

    // Save channel and reboot (same as original logic)
    prefs.putUInt(K_VIDEO_INDEX, currentChannel);
    prefs.end();

    esp_restart();
}
