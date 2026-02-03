/*
 * Author: embedCrest Technology Private Limited
 */
// -------------------------------------------------------
// SPIFFS directory dump utility
// -------------------------------------------------------

#include "SPIFFS.h"

// -------------------------------------------------------
// Forward declarations
// -------------------------------------------------------
static void dumpDirectory(const char* path);
static void listDir(const char* dirPath);

// -------------------------------------------------------
// Setup & SPIFFS initialization
// -------------------------------------------------------
void setup() {
  Serial.begin(115200);
  while (!Serial) {
    ; // wait for serial port
  }

  Serial.println();
  Serial.println("=== SPIFFS File Browser ===");

  // Mount the SPIFFS filesystem
  if (!SPIFFS.begin()) {
    Serial.println("SPIFFS initialization failed!");
    Serial.println("Halting here, filesystem not available.");
    while (true) {
      yield();
    }
  }

  Serial.println("\r\nFilesystem mounted OK.");
  Serial.println("Scanning root directory: /");

  // Use wrapper with same semantic role as original listDir
  listDir("/");
}

void loop() {
  // Nothing to do here – one-shot directory dump in setup
  delay(1000);
}

// -------------------------------------------------------
// Notification handler (not used, placeholder for style)
// -------------------------------------------------------
// Keeping this section title to match your requested comment
// pattern. No notification logic is required in this sketch.

// -------------------------------------------------------
// Directory listing helpers
// -------------------------------------------------------
static void listDir(const char* dirPath) {
  // Thin wrapper to keep naming compatible if reused elsewhere
  dumpDirectory(dirPath);
}

static void dumpDirectory(const char* path) {
  File root = SPIFFS.open(path);
  if (!root || !root.isDirectory()) {
    Serial.print("Unable to open directory: ");
    Serial.println(path);
    return;
  }

  File entry = root.openNextFile();
  if (!entry) {
    Serial.println("Directory is empty.");
    return;
  }

  while (entry) {
    Serial.print("FILE: ");
    Serial.println(entry.name());
    entry = root.openNextFile();
  }
}
