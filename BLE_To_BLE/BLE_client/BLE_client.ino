/*
 *BLE client implementation 
 * 
 * Author: EmbedCrest Technology Private Limited
 */

// -------------------------------------------------------
// Includes & Global BLE Objects
// -------------------------------------------------------
#include "BLEDevice.h"
//#include "BLEScan.h"

// -------------------------------------------------------
// Remote Service / Characteristic UUIDs
// -------------------------------------------------------
static BLEUUID kTargetServiceUuid("4fafc201-1fb5-459e-8fcc-c5c9c331914b");
static BLEUUID kTargetCharUuid   ("beb5483e-36e1-4688-b7f5-ea07361b26a8");

// -------------------------------------------------------
// Connection / State Flags
// -------------------------------------------------------
static boolean                   g_shouldConnect  = false;
static boolean                   g_isConnected    = false;
static boolean                   g_shouldScan     = false;
static BLERemoteCharacteristic*  g_remoteChar     = nullptr;
static BLEAdvertisedDevice*      g_targetDevice   = nullptr;

// -------------------------------------------------------
// Notification Handler (from Remote Characteristic)
// -------------------------------------------------------
static void notifyCallback(
  BLERemoteCharacteristic* pRemote,
  uint8_t*                 pData,
  size_t                   length,
  bool                     isNotify)
{
  (void)isNotify;
  Serial.print("Notify from characteristic ");
  Serial.print(pRemote->getUUID().toString().c_str());
  Serial.print(" length ");
  Serial.println(length);
  Serial.print("Data: ");
  Serial.println(reinterpret_cast<char*>(pData));
}

// -------------------------------------------------------
// BLE Client Connection Callbacks
// -------------------------------------------------------
class ClientEventHandler : public BLEClientCallbacks {
  void onConnect(BLEClient* cli) override {
    (void)cli;
    Serial.println("Client connected");
  }

  void onDisconnect(BLEClient* cli) override {
    (void)cli;
    g_isConnected = false;
    Serial.println("Client disconnected");
  }
};

// -------------------------------------------------------
// Connect to Server & Resolve Remote Characteristic
// -------------------------------------------------------
bool connectToServer() {
  Serial.print("Forming a connection to ");
  Serial.println(g_targetDevice->getAddress().toString().c_str());

  // Create client instance
  BLEClient* g_client = BLEDevice::createClient();
  Serial.println(" - Client created");

  g_client->setClientCallbacks(new ClientEventHandler());

  // Establish link with remote server
  g_client->connect(g_targetDevice);  // address type auto-detected
  Serial.println(" - Connected to server");

  // Obtain remote service
  BLERemoteService* pRemoteService = g_client->getService(kTargetServiceUuid);
  if (pRemoteService == nullptr) {
    Serial.print("Failed to find service UUID: ");
    Serial.println(kTargetServiceUuid.toString().c_str());
    g_client->disconnect();
    return false;
  }
  Serial.println(" - Found service");

  // Obtain remote characteristic
  g_remoteChar = pRemoteService->getCharacteristic(kTargetCharUuid);
  if (g_remoteChar == nullptr) {
    Serial.print("Failed to find characteristic UUID: ");
    Serial.println(kTargetCharUuid.toString().c_str());
    g_client->disconnect();
    return false;
  }
  Serial.println(" - Found characteristic");

  // Optional initial read for debug
  if (g_remoteChar->canRead()) {
    std::string value = g_remoteChar->readValue();
    Serial.print("Initial characteristic value: ");
    Serial.println(value.c_str());
  }

  // Subscribe for notifications if supported by remote
  if (g_remoteChar->canNotify()) {
    g_remoteChar->registerForNotify(notifyCallback);
  }

  g_isConnected = true;
  return true;
}

// -------------------------------------------------------
// Scan Result Callback (device discovery)
// -------------------------------------------------------
class ScanResultHandler : public BLEAdvertisedDeviceCallbacks {
  void onResult(BLEAdvertisedDevice advertisedDevice) override {
    Serial.print("BLE Advertised Device: ");
    Serial.println(advertisedDevice.toString().c_str());

    // Pick the first matching / visible device as target
    g_targetDevice = new BLEAdvertisedDevice(advertisedDevice);
    BLEDevice::getScan()->stop();
    g_shouldConnect = true;
    g_shouldScan    = true;
  }
};

// -------------------------------------------------------
// Setup: Initialize BLE & Scanner
// -------------------------------------------------------
void setup() {
  Serial.begin(115200);
  Serial.println("Starting BLE client...");

  // Initialize BLE stack with generic device name
  BLEDevice::init("");

  // Configure active scanning
  BLEScan* scan = BLEDevice::getScan();
  scan->setAdvertisedDeviceCallbacks(new ScanResultHandler());
  scan->setInterval(1349);
  scan->setWindow(449);
  scan->setActiveScan(true);
  scan->start(5, false);
}

// -------------------------------------------------------
// Periodic Data Writer (Client -> Server)
// -------------------------------------------------------
static void pushPeriodicPayload()
{
  String newValue = "Time since boot: " + String(millis() / 1000);
  Serial.println("Updating characteristic to: \"" + newValue + "\"");
  g_remoteChar->writeValue(newValue.c_str(), newValue.length());
}

// -------------------------------------------------------
// Main Loop: Connection Handling & Data Exchange
// -------------------------------------------------------
void loop() {
  // One-shot connect once scan callback has set target
  if (g_shouldConnect == true) {
    if (connectToServer()) {
      Serial.println("Client successfully connected to server");
    } else {
      Serial.println("Connection to server failed; no further attempts in this example");
    }
    g_shouldConnect = false;
  }

  // When connected, periodically write data to remote characteristic
  if (g_isConnected) {
    pushPeriodicPayload();
  } else if (g_shouldScan) {
    // Restart passive scanning after disconnect
    BLEDevice::getScan()->start(0);
  }

  delay(1000);
}

// -------------------------------------------------------
// End of File
// -------------------------------------------------------
