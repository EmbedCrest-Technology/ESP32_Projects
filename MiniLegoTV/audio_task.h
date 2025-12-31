/* 
 * Author: EmbedCrest Technology Private Limited
/*
#pragma once

#include "driver/i2s.h"
#include "AACDecoderHelix.h"
#include "MP3DecoderHelix.h"

// -------------------------------------------------------
// Profiling counters (audio path)
// -------------------------------------------------------
static unsigned long g_msReadAudio   = 0;
static unsigned long g_msDecodeAudio = 0;
static unsigned long g_msPlayAudio   = 0;

// -------------------------------------------------------
// I2S driver state
// -------------------------------------------------------
static i2s_port_t   g_i2sPort;
static int          g_currentSampleRate = 0;

// -------------------------------------------------------
// I2S low-level init
// -------------------------------------------------------
static esp_err_t i2s_init(i2s_port_t port,
                          uint32_t sample_rate,
                          int pinMCK,
                          int pinBCLK,
                          int pinLRCLK,
                          int pinDOUT,
                          int pinDIN)
{
    g_i2sPort = port;

    i2s_config_t cfg;
    cfg.mode                 = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_TX);
    cfg.sample_rate          = sample_rate;
    cfg.bits_per_sample      = I2S_BITS_PER_SAMPLE_16BIT;
    cfg.channel_format       = I2S_CHANNEL_FMT_RIGHT_LEFT;
    cfg.communication_format = I2S_COMM_FORMAT_STAND_I2S;
    cfg.intr_alloc_flags     = ESP_INTR_FLAG_LEVEL1;
    cfg.dma_buf_count        = 8;
    cfg.dma_buf_len          = 160;
    cfg.use_apll             = false;
    cfg.tx_desc_auto_clear   = true;
    cfg.fixed_mclk           = 0;
    cfg.mclk_multiple        = I2S_MCLK_MULTIPLE_DEFAULT;
    cfg.bits_per_chan        = I2S_BITS_PER_CHAN_16BIT;

    i2s_pin_config_t pins;
    pins.mck_io_num   = pinMCK;
    pins.bck_io_num   = pinBCLK;
    pins.ws_io_num    = pinLRCLK;
    pins.data_out_num = pinDOUT;
    pins.data_in_num  = pinDIN;

    esp_err_t rc = ESP_OK;
    rc |= i2s_driver_install(port, &cfg, 0, nullptr);
    rc |= i2s_set_pin(port, &pins);
    return rc;
}

// -------------------------------------------------------
// Notification handler
// Audio sample push helpers (AAC / MP3 callbacks)
// -------------------------------------------------------
static void aacAudioDataCallback(AACFrameInfo &info,
                                 int16_t *pcm,
                                 size_t sampleCount)
{
    unsigned long t0 = millis();

    if (g_currentSampleRate != info.sampRateOut)
    {
        i2s_set_clk(
            g_i2sPort,
            info.sampRateOut,
            info.bitsPerSample,
            (info.nChans == 2) ? I2S_CHANNEL_STEREO : I2S_CHANNEL_MONO);

        g_currentSampleRate = info.sampRateOut;
    }

    size_t bytesWritten = 0;
    i2s_write(g_i2sPort, pcm, sampleCount * sizeof(int16_t), &bytesWritten, portMAX_DELAY);

    g_msPlayAudio += millis() - t0;
}

static void mp3AudioDataCallback(MP3FrameInfo &info,
                                 int16_t *pcm,
                                 size_t sampleCount)
{
    unsigned long t0 = millis();

    if (g_currentSampleRate != info.samprate)
    {
        log_i("bitrate:%d, ch:%d, sr:%d, bits:%d, out:%d, layer:%d, ver:%d",
              info.bitrate, info.nChans, info.samprate,
              info.bitsPerSample, info.outputSamps,
              info.layer, info.version);

        i2s_set_clk(
            g_i2sPort,
            info.samprate,
            info.bitsPerSample,
            (info.nChans == 2) ? I2S_CHANNEL_STEREO : I2S_CHANNEL_MONO);

        g_currentSampleRate = info.samprate;
    }

    size_t bytesWritten = 0;
    i2s_write(g_i2sPort, pcm, sampleCount * sizeof(int16_t), &bytesWritten, portMAX_DELAY);

    g_msPlayAudio += millis() - t0;
}

// -------------------------------------------------------
// Internal decode frame buffer
// -------------------------------------------------------
static uint8_t g_frameBuffer[MP3_MAX_FRAME_SIZE];

// -------------------------------------------------------
// AAC decoding task
// -------------------------------------------------------
static libhelix::AACDecoderHelix g_aacDecoder(aacAudioDataCallback);

static void aac_player_task(void *param)
{
    Stream *input = static_cast<Stream *>(param);
    int bytesRead;
    int bytesUsed;
    unsigned long t0 = millis();

    while ((bytesRead = input->readBytes(g_frameBuffer, sizeof(g_frameBuffer))) > 0)
    {
        g_msReadAudio += millis() - t0;
        t0 = millis();

        int remaining = bytesRead;
        while (remaining > 0)
        {
            bytesUsed = g_aacDecoder.write(g_frameBuffer, remaining);
            remaining -= bytesUsed;
        }

        g_msDecodeAudio += millis() - t0;
        t0 = millis();
    }

    log_i("AAC stop.");
    vTaskDelete(nullptr);
}

// -------------------------------------------------------
// MP3 decoding task
// -------------------------------------------------------
static libhelix::MP3DecoderHelix g_mp3Decoder(mp3AudioDataCallback);

static void mp3_player_task(void *param)
{
    Stream *input = static_cast<Stream *>(param);
    int bytesRead;
    int bytesUsed;
    unsigned long t0 = millis();

    while ((bytesRead = input->readBytes(g_frameBuffer, sizeof(g_frameBuffer))) > 0)
    {
        g_msReadAudio += millis() - t0;
        t0 = millis();

        int remaining = bytesRead;
        while (remaining > 0)
        {
            bytesUsed = g_mp3Decoder.write(g_frameBuffer, remaining);
            remaining -= bytesUsed;
        }

        g_msDecodeAudio += millis() - t0;
        t0 = millis();
    }

    log_i("MP3 stop.");
    vTaskDelete(nullptr);
}

// -------------------------------------------------------
// Notification handler
// Task starters (called from .ino)
// -------------------------------------------------------
static BaseType_t aac_player_task_start(Stream *input, BaseType_t core)
{
    g_aacDecoder.begin();

    return xTaskCreatePinnedToCore(
        aac_player_task,
        "AAC Player Task",
        2000,
        input,
        configMAX_PRIORITIES - 1,
        nullptr,
        core);
}

static BaseType_t mp3_player_task_start(Stream *input, BaseType_t core)
{
    g_mp3Decoder.begin();

    return xTaskCreatePinnedToCore(
        mp3_player_task,
        "MP3 Player Task",
        2000,
        input,
        configMAX_PRIORITIES - 1,
        nullptr,
        core);
}
