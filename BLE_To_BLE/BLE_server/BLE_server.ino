
/* BLE notification server  
 * 
 * Author: EmbedCrest Technology Private Limited
 *
 */

// -------------------------------------------------------
// Includes & Global Objects
// -------------------------------------------------------
#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLEUtils.h>
#include <BLE2902.h>

BLEServer*         g_server         = nullptr;
BLECharacteristic* g_notifyChar     = nullptr;
bool               g_linkActive     = false;
bool               g_prevLinkActive = false;
uint32_t           g_txCounter      = 0;

// -------------------------------------------------------
// BLE UUID Definitions
// -------------------------------------------------------
#define kBleServiceUuid "4fafc201-1fb5-459e-8fcc-c5c9c331914b"
#define kBleCharUuid    "beb5483e-36e1-4688-b7f5-ea07361b26a8"

// -------------------------------------------------------
// Server Callback Implementation
// -------------------------------------------------------
class ServerEventHandler : public BLEServerCallbacks {
  void onConnect(BLEServer* srv) override {
    (void)srv;
    g_linkActive = true;

    // Optionally keep advertising while connected
    BLEDevice::startAdvertising();
  }

  void onDisconnect(BLEServer* srv) override {
    (void)srv;
    g_linkActive = false;
  }
};

// -------------------------------------------------------
// Advertising Configuration
// -------------------------------------------------------
static void configureAdvertising()
{
  BLEAdvertising* adv = BLEDevice::getAdvertising();
  adv->addServiceUUID(kBleServiceUuid);
  adv->setScanResponse(false);
  adv->setMinPreferred(0x00);          // do not advertise preferred connection parameters

  BLEDevice::startAdvertising();
  Serial.println("Waiting for a client connection to notify...");
}

// -------------------------------------------------------
// Setup: BLE Initialization & Service Creation
// -------------------------------------------------------
void setup() {
  Serial.begin(115200);

  // Initialize BLE stack with device name
  BLEDevice::init("ESP32 THAT PROJECT");

  // Create BLE server instance
  g_server = BLEDevice::createServer();
  g_server->setCallbacks(new ServerEventHandler());

  // Create primary BLE service
  BLEService* pService = g_server->createService(kBleServiceUuid);

  // Create characteristic (read / write / notify / indicate)
  g_notifyChar = pService->createCharacteristic(
                    kBleCharUuid,
                    BLECharacteristic::PROPERTY_READ   |
                    BLECharacteristic::PROPERTY_WRITE  |
                    BLECharacteristic::PROPERTY_NOTIFY |
                    BLECharacteristic::PROPERTY_INDICATE
                 );

  // Add CCCD descriptor to enable notifications on client side
  g_notifyChar->addDescriptor(new BLE2902());

  // Start service and begin advertising
  pService->start();
  configureAdvertising();
}

// -------------------------------------------------------
// Notification Handling (Tx Counter Updates)
// -------------------------------------------------------
static void sendCounterNotification()
{
  // Push current counter value as raw bytes
  g_notifyChar->setValue(reinterpret_cast<uint8_t*>(&g_txCounter), sizeof(g_txCounter));
  g_notifyChar->notify();
  g_txCounter++;

  // Note: lowering this delay may congest the BLE stack; 500 ms is safe for demo
  delay(500);
}

// -------------------------------------------------------
// Loop: Connection State Handling & Notifications
// -------------------------------------------------------
void loop() {
  // When a client is connected, periodically send notifications
  if (g_linkActive) {
    sendCounterNotification();
  }

  // Transition: connected -> disconnected
  if (!g_linkActive && g_prevLinkActive) {
    // Give BLE stack a bit of time to settle before re-advertising
    delay(500);
    g_server->startAdvertising();
    Serial.println("Restart advertising");
    g_prevLinkActive = g_linkActive;
  }

  // Transition: disconnected -> connected
  if (g_linkActive && !g_prevLinkActive) {
    // Hook point for any on-(re)connect logic if needed
    g_prevLinkActive = g_linkActive;
  }
}

// -------------------------------------------------------
// End of File
// -------------------------------------------------------
