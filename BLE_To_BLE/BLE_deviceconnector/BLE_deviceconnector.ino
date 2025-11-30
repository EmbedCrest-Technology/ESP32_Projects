/* BLE Deviceconnector
 * 
 * Author: EmbedCrest Technology Private Limited
 *
 */

// -------------------------------------------------------
// TFT Includes & Display Configuration
// -------------------------------------------------------
#include "Adafruit_GFX.h"
#include "Adafruit_ILI9341.h"

#define TFT_DC   17
#define TFT_CS   5
#define TFT_MOSI 23
#define TFT_CLK  18
#define TFT_RST  4
#define TFT_MISO 19

static Adafruit_ILI9341 lcd = Adafruit_ILI9341(TFT_CS, TFT_DC, TFT_RST);
static int  kRowOffset      = 30;
static int  g_numDiscovered = 0;
static int  marginY         = 6;

// -------------------------------------------------------
// User Input (Button) Definitions
// -------------------------------------------------------
#define PIN_NEXT   12
#define PIN_SELECT 14

static int g_cursorIndex = 1;
static int g_lastCursor  = 1;

// -------------------------------------------------------
// BLE Includes & UUID Configuration
// -------------------------------------------------------
#include "BLEDevice.h"
//#include "BLEScan.h"

// BLE target UUIDs (must match remote server)
static BLEUUID kTargetServiceUuid("4fafc201-1fb5-459e-8fcc-c5c9c331914b");
static BLEUUID kTargetCharUuid   ("beb5483e-36e1-4688-b7f5-ea07361b26a8");

// -------------------------------------------------------
// BLE State Variables
// -------------------------------------------------------
static boolean                  g_shouldConnect  = false;
static boolean                  g_isConnected    = false;
static boolean                  g_shouldScan     = false;
static BLERemoteCharacteristic* g_remoteChar     = nullptr;
static BLEAdvertisedDevice*     g_selectedDevice = nullptr;

// Storage for scan results to support UI selection
static BLEAdvertisedDevice results[10];

// -------------------------------------------------------
// Notification Callback (remote -> local)
// -------------------------------------------------------
static void handleNotification(
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
// Client Connection Callbacks
// -------------------------------------------------------
class ClientEventHandler : public BLEClientCallbacks {
  void onConnect(BLEClient* cli) override {
    (void)cli;
    Serial.println("onConnect");
  }

  void onDisconnect(BLEClient* cli) override {
    (void)cli;
    g_isConnected = false;
    Serial.println("onDisconnect");
  }
};

// -------------------------------------------------------
// Establish BLE Connection & Resolve Characteristic
// -------------------------------------------------------
bool establishConnection() {
  Serial.print("Forming a connection to ");
  Serial.println(g_selectedDevice->getAddress().toString().c_str());

  BLEClient* pClient = BLEDevice::createClient();
  Serial.println(" - Created client");

  pClient->setClientCallbacks(new ClientEventHandler());

  // Connect to the remote BLE server
  pClient->connect(g_selectedDevice);  // address type auto-detected
  Serial.println(" - Connected to server");

  // Obtain a reference to the remote service
  BLERemoteService* pRemoteService = pClient->getService(kTargetServiceUuid);
  if (pRemoteService == nullptr) {
    Serial.print("Failed to find our service UUID: ");
    Serial.println(kTargetServiceUuid.toString().c_str());
    pClient->disconnect();
    return false;
  }
  Serial.println(" - Found our service");

  // Obtain the characteristic
  g_remoteChar = pRemoteService->getCharacteristic(kTargetCharUuid);
  if (g_remoteChar == nullptr) {
    Serial.print("Failed to find our characteristic UUID: ");
    Serial.println(kTargetCharUuid.toString().c_str());
    pClient->disconnect();
    return false;
  }
  Serial.println(" - Found our characteristic");

  // Optional read for initial state / debugging
  if (g_remoteChar->canRead()) {
    std::string value = g_remoteChar->readValue();
    Serial.print("The characteristic value was: ");
    Serial.println(value.c_str());
  }

  // Subscribe to notifications when available
  if (g_remoteChar->canNotify()) {
    g_remoteChar->registerForNotify(handleNotification);
  }

  g_isConnected = true;
  return true;
}

// -------------------------------------------------------
// Scan Result Callback (populate TFT list)
// -------------------------------------------------------
/**
 * Scan for BLE servers and feed results into TFT UI.
 */
class ScanResultHandler : public BLEAdvertisedDeviceCallbacks {
public:
  ScanResultHandler() {
    renderStatus("SEARCHING...");
  }

  void onResult(BLEAdvertisedDevice advertisedDevice) override {
    renderStatus("...RESULT");

    Serial.print("BLE Advertised Device found: ");
    // Serial.println(advertisedDevice.toString().c_str());

    String deviceName = advertisedDevice.getName().c_str();
    if (deviceName.length() > 1 && g_numDiscovered < 10) {
      // Cache device for later selection via buttons
      results[g_numDiscovered] = advertisedDevice;
      g_numDiscovered++;
      renderDeviceRow(deviceName);
    }

    // Original sample also showed how to filter by service UUID.
    // That logic is omitted here and replaced by manual selection
    // through the TFT UI (see buttonsCallBack()).
  }
};

// -------------------------------------------------------
// Forward Declarations for UI Helpers
// -------------------------------------------------------
void renderStatus(const String& statusText);
void renderDeviceRow(const String& deviceName);
void renderConnectedView(String deviceName);
void renderPayload(String payload);
void buttonsCallBack();

// -------------------------------------------------------
// Setup: TFT Init + BLE Init + Start Scan
// -------------------------------------------------------
void setup() {
  Serial.begin(115200);
  Serial.println("Starting Arduino BLE Client with TFT UI...");

  // TFT setup
  lcd.begin();
  lcd.setRotation(0);
  lcd.fillScreen(ILI9341_WHITE);

  // Configure button inputs
  pinMode(PIN_NEXT, INPUT);
  pinMode(PIN_SELECT, INPUT);

  // BLE init
  BLEDevice::init("");

  // Scanner configuration
  BLEScan* pBLEScan = BLEDevice::getScan();
  pBLEScan->setAdvertisedDeviceCallbacks(new ScanResultHandler());
  pBLEScan->setInterval(1349);
  pBLEScan->setWindow(449);
  pBLEScan->setActiveScan(true);
  pBLEScan->start(3, false);
}

// -------------------------------------------------------
// Main Loop: Connection Handling & TFT Navigation
// -------------------------------------------------------
void loop() {
  // Handle pending connection request (user selection)
  if (g_shouldConnect == true) {
    if (establishConnection()) {
      renderConnectedView(g_selectedDevice->getName().c_str());
      Serial.println("We are now connected to the BLE Server.");
    } else {
      Serial.println("Failed to connect to the server; stopping further attempts.");
    }
    g_shouldConnect = false;
  }

  // If we are connected, periodically push data to remote characteristic
  if (g_isConnected) {
    String newValue = "Time since boot: " + String(millis() / 1000);
    Serial.println("Setting new characteristic value to \"" + newValue + "\"");

    g_remoteChar->writeValue(newValue.c_str(), newValue.length());
    renderPayload(newValue);
  } else if (g_shouldScan) {
    // Pattern to restart scanning after disconnect
    BLEDevice::getScan()->start(0);
  }

  // Handle UI buttons and cursor movement
  buttonsCallBack();
  delay(250);
}

// -------------------------------------------------------
// Button Handling & TFT Highlighting Logic
// -------------------------------------------------------
void buttonsCallBack() {
  int nextButtonState   = digitalRead(PIN_NEXT);
  int selectButtonState = digitalRead(PIN_SELECT);

  // "Select" button: choose highlighted device and trigger connect
  if (selectButtonState == HIGH && g_cursorIndex > 0 && g_cursorIndex <= g_numDiscovered) {
    g_selectedDevice = new BLEAdvertisedDevice(results[g_cursorIndex - 1]);
    g_shouldConnect  = true;

    int drawingOffsetY = kRowOffset * g_lastCursor + 8;
    lcd.drawCircle(6, drawingOffsetY, 6, ILI9341_WHITE);
  }

  // "Next" button: move cursor to next discovered device
  if (nextButtonState == HIGH) {
    g_cursorIndex++;
  }

  // Wrap-around cursor index when exceeding list
  if (g_cursorIndex > g_numDiscovered) {
    g_cursorIndex = 1;
  }

  // Only show selection cursor when not connected
  if (!g_isConnected) {
    if (g_lastCursor != g_cursorIndex) {
      int drawingOffsetY = kRowOffset * g_lastCursor + 8;
      lcd.drawCircle(6, drawingOffsetY, 6, ILI9341_WHITE);
    }

    if (g_cursorIndex != 0) {
      int drawingOffsetY = kRowOffset * g_cursorIndex + 8;
      lcd.drawCircle(6, drawingOffsetY, 6, ILI9341_RED);

      g_lastCursor = g_cursorIndex;
    }
  }
}

// -------------------------------------------------------
// TFT Rendering Helpers
// -------------------------------------------------------
void renderStatus(const String& statusText) {
  lcd.fillRect(0, 0, 240, 30, ILI9341_DARKGREY);
  lcd.setCursor(6, 6);
  lcd.setTextColor(ILI9341_WHITE);
  lcd.setTextSize(2);
  lcd.print(statusText);
}

void renderDeviceRow(const String& deviceName) {
  int y = kRowOffset * g_numDiscovered + marginY;
  lcd.setCursor(18, y);
  lcd.setTextColor(ILI9341_BLACK);
  lcd.setTextSize(2);
  lcd.print(deviceName);

  // Highlight first discovered device by default
  if (g_numDiscovered == 1) {
    int drawingOffsetY = kRowOffset * g_numDiscovered + 8;
    lcd.drawCircle(6, drawingOffsetY, 6, ILI9341_RED);
  }
}

void renderConnectedView(String deviceName) {
  lcd.fillRect(0, 0, 240, 320, ILI9341_WHITE);
  lcd.fillRect(0, 0, 240, 30, ILI9341_DARKGREY);
  lcd.setCursor(6, 6);
  lcd.setTextColor(ILI9341_WHITE);
  lcd.setTextSize(2);
  lcd.print("Connected");

  lcd.setCursor(6, 36);
  lcd.setTextColor(ILI9341_BLACK);
  lcd.setTextSize(2);
  lcd.print(deviceName);
}

void renderPayload(String notiData) {
  lcd.fillRect(0, 50, 240, 100, ILI9341_WHITE);
  lcd.setCursor(6, 70);
  lcd.setTextColor(ILI9341_RED);
  lcd.setTextSize(2);
  lcd.print(notiData);
}

// -------------------------------------------------------
// End of File
// -------------------------------------------------------
