 
 /* 
  * Author: EmbedCrest Technology Private Limited
  */

#pragma once

// -------------------------------------------------------
// MJPEG decode / draw task configuration
// -------------------------------------------------------
#define READ_BUFFER_SIZE         1024
#define MAXOUTPUTSIZE            (288 / 3 / 16)   // MCU tiles limit
#define NUMBER_OF_DECODE_BUFFER  3
#define NUMBER_OF_DRAW_BUFFER    9

#include <FS.h>
#include <JPEGDEC.h>

// -------------------------------------------------------
// Data structures
// -------------------------------------------------------
typedef struct
{
  int32_t  size;
  uint8_t *buf;
} MjpegChunk;

typedef struct
{
  xQueueHandle        queue;
  JPEG_DRAW_CALLBACK *drawFunc;
} DrawTaskParam;

typedef struct
{
  xQueueHandle        queue;
  MjpegChunk         *chunk;
  JPEG_DRAW_CALLBACK *drawFunc;
} DecodeTaskParam;

// -------------------------------------------------------
// Internal state
// -------------------------------------------------------
static JPEGDRAW      g_drawSlots[NUMBER_OF_DRAW_BUFFER];
static int           g_drawIndex         = 0;
static JPEGDEC       g_jpeg;
static xQueueHandle  g_drawQueue        = nullptr;
static bool          g_bigEndian        = false;

static unsigned long g_msVideoRead      = 0;
static unsigned long g_msVideoDecode    = 0;
static unsigned long g_msVideoShow      = 0;

static Stream       *g_input            = nullptr;
static int32_t       g_chunkCapacity    = 0;

static uint8_t      *g_readBuf          = nullptr;
static int32_t       g_mjpegOffset      = 0;

static TaskHandle_t  g_decodeTask       = nullptr;
static TaskHandle_t  g_drawTask         = nullptr;

static DecodeTaskParam g_decodeParam;
static DrawTaskParam   g_drawParam;

static uint8_t      *g_activeMjpegBuf   = nullptr;
static uint8_t       g_activeChunkIdx   = 0;

static int32_t       g_inputIndex       = 0;
static int32_t       g_bytesInReadBuf   = 0;
static int32_t       g_unused           = 0;

static MjpegChunk    g_chunks[NUMBER_OF_DECODE_BUFFER];

// -------------------------------------------------------
// Notification handler
// Copy MCU block into queued draw buffer
// -------------------------------------------------------
static int queueDrawMCU(JPEGDRAW *pDraw)
{
  int bytes = pDraw->iWidth * pDraw->iHeight * 2;

  JPEGDRAW *slot = &g_drawSlots[g_drawIndex % NUMBER_OF_DRAW_BUFFER];
  slot->x        = pDraw->x;
  slot->y        = pDraw->y;
  slot->iWidth   = pDraw->iWidth;
  slot->iHeight  = pDraw->iHeight;

  memcpy(slot->pPixels, pDraw->pPixels, bytes);

  ++g_drawIndex;
  xQueueSend(g_drawQueue, &slot, portMAX_DELAY);

  return 1;
}

// -------------------------------------------------------
// Decode task – converts MJPEG chunk to RGB tiles
// -------------------------------------------------------
static void decode_task(void *arg)
{
  DecodeTaskParam *p = (DecodeTaskParam *)arg;
  MjpegChunk      *chunk;

  log_i("decode_task start.");

  while (xQueueReceive(p->queue, &chunk, portMAX_DELAY))
  {
    unsigned long t0 = millis();

    g_jpeg.openRAM(chunk->buf, chunk->size, p->drawFunc);

    if (g_bigEndian)
    {
      g_jpeg.setPixelType(RGB565_BIG_ENDIAN);
    }

    g_jpeg.setMaxOutputSize(MAXOUTPUTSIZE);
    g_jpeg.decode(0, 0, 0);
    g_jpeg.close();

    g_msVideoDecode += millis() - t0;
  }

  vQueueDelete(p->queue);
  log_i("decode_task end.");
  vTaskDelete(nullptr);
}

// -------------------------------------------------------
// Draw task – pushes RGB tiles to user callback
// -------------------------------------------------------
static void draw_task(void *arg)
{
  DrawTaskParam *p = (DrawTaskParam *)arg;
  JPEGDRAW      *blk;

  log_i("draw_task start.");

  while (xQueueReceive(p->queue, &blk, portMAX_DELAY))
  {
    p->drawFunc(blk);
  }

  vQueueDelete(p->queue);
  log_i("draw_task end.");
  vTaskDelete(nullptr);
}

// -------------------------------------------------------
// MJPEG subsystem setup
// -------------------------------------------------------
static bool mjpeg_setup(Stream *input,
                        int32_t mjpegBufSize,
                        JPEG_DRAW_CALLBACK *drawFn,
                        bool useBigEndian,
                        BaseType_t decodeCore,
                        BaseType_t drawCore)
{
  g_input         = input;
  g_chunkCapacity = mjpegBufSize;
  g_bigEndian     = useBigEndian;

  // Allocate decode chunks
  for (int i = 0; i < NUMBER_OF_DECODE_BUFFER; ++i)
  {
    g_chunks[i].buf = (uint8_t *)malloc(mjpegBufSize);
    if (g_chunks[i].buf)
      log_i("#%d decode buffer allocated.", i);
    else
      log_e("#%d decode buffer alloc failed.", i);
  }

  g_activeMjpegBuf = g_chunks[g_activeChunkIdx].buf;

  if (!g_readBuf)
  {
    g_readBuf = (uint8_t *)malloc(READ_BUFFER_SIZE);
  }
  if (g_readBuf)
  {
    log_i("Read buffer allocated.");
  }

  // Queues
  g_drawQueue          = xQueueCreate(NUMBER_OF_DRAW_BUFFER, sizeof(JPEGDRAW));
  g_drawParam.queue    = g_drawQueue;
  g_drawParam.drawFunc = drawFn;

  g_decodeParam.queue    = xQueueCreate(NUMBER_OF_DECODE_BUFFER, sizeof(MjpegChunk));
  g_decodeParam.drawFunc = queueDrawMCU;

  // Spawn tasks
  xTaskCreatePinnedToCore(
      decode_task,
      "MJPEG Decode Task",
      2000,
      &g_decodeParam,
      configMAX_PRIORITIES - 1,
      &g_decodeTask,
      decodeCore);

  xTaskCreatePinnedToCore(
      draw_task,
      "MJPEG Draw Task",
      2000,
      &g_drawParam,
      configMAX_PRIORITIES - 1,
      &g_drawTask,
      drawCore);

  // Allocate draw buffers
  for (int i = 0; i < NUMBER_OF_DRAW_BUFFER; ++i)
  {
    if (!g_drawSlots[i].pPixels)
    {
      g_drawSlots[i].pPixels =
          (uint16_t *)heap_caps_malloc(MAXOUTPUTSIZE * 16 * 16 * 2, MALLOC_CAP_DMA);
    }

    if (g_drawSlots[i].pPixels)
      log_i("#%d draw buffer allocated.", i);
    else
      log_e("#%d draw buffer alloc failed.", i);
  }

  return true;
}

// -------------------------------------------------------
// Notification handler
// Parse one MJPEG frame from stream into active buffer
// -------------------------------------------------------
static bool mjpeg_read_frame()
{
  if (g_inputIndex == 0)
  {
    g_bytesInReadBuf = g_input->readBytes(g_readBuf, READ_BUFFER_SIZE);
    g_inputIndex    += g_bytesInReadBuf;
  }

  g_mjpegOffset = 0;
  int  i        = 0;
  bool haveFFD8 = false;

  // Search for JPEG header (FFD8)
  while ((g_bytesInReadBuf > 0) && (!haveFFD8))
  {
    i = 0;
    while ((i < g_bytesInReadBuf) && (!haveFFD8))
    {
      if ((g_readBuf[i] == 0xFF) && (g_readBuf[i + 1] == 0xD8))
      {
        haveFFD8 = true;
      }
      ++i;
    }

    if (haveFFD8)
    {
      --i;
    }
    else
    {
      g_bytesInReadBuf = g_input->readBytes(g_readBuf, READ_BUFFER_SIZE);
    }
  }

  uint8_t *p = g_readBuf + i;
  g_bytesInReadBuf -= i;

  bool haveFFD9 = false;

  if (g_bytesInReadBuf > 0)
  {
    i = 3;
    while ((g_bytesInReadBuf > 0) && (!haveFFD9))
    {
      // Detect JPEG trailer (FFD9)
      if ((g_mjpegOffset > 0) &&
          (g_activeMjpegBuf[g_mjpegOffset - 1] == 0xFF) &&
          (p[0] == 0xD9))
      {
        haveFFD9 = true;
      }
      else
      {
        while ((i < g_bytesInReadBuf) && (!haveFFD9))
        {
          if ((p[i] == 0xFF) && (p[i + 1] == 0xD9))
          {
            haveFFD9 = true;
            ++i;
          }
          ++i;
        }
      }

      memcpy(g_activeMjpegBuf + g_mjpegOffset, p, i);
      g_mjpegOffset += i;

      int32_t leftover = g_bytesInReadBuf - i;
      if (leftover > 0)
      {
        memcpy(g_readBuf, p + i, leftover);
        g_bytesInReadBuf = g_input->readBytes(g_readBuf + leftover,
                                              READ_BUFFER_SIZE - leftover);
        p               = g_readBuf;
        g_inputIndex   += g_bytesInReadBuf;
        g_bytesInReadBuf += leftover;
      }
      else
      {
        g_bytesInReadBuf = g_input->readBytes(g_readBuf, READ_BUFFER_SIZE);
        p               = g_readBuf;
        g_inputIndex   += g_bytesInReadBuf;
      }
      i = 0;
    }

    if (haveFFD9)
    {
      if (g_mjpegOffset > g_chunkCapacity)
      {
        log_e("MJPEG frame too large: %d > %d",
              g_mjpegOffset, g_chunkCapacity);
      }
      return true;
    }
  }

  return false;
}

// -------------------------------------------------------
// Queue current MJPEG frame to decode task
// -------------------------------------------------------
static bool mjpeg_draw_frame()
{
  MjpegChunk *chunk = &g_chunks[g_activeChunkIdx];
  chunk->size       = g_mjpegOffset;

  xQueueSend(g_decodeParam.queue, &chunk, portMAX_DELAY);

  // Switch to next decode buffer
  ++g_activeChunkIdx;
  if (g_activeChunkIdx >= NUMBER_OF_DECODE_BUFFER)
  {
    g_activeChunkIdx = 0;
  }
  g_activeMjpegBuf = g_chunks[g_activeChunkIdx].buf;

  return true;
}
