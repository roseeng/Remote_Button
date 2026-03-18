/*
 * BLE Client 
 *
 * This example demonstrates how to run BLE client 
 * functionality on the same ESP32 device simultaneously.
 *
 * The device will:
 
 * - Act as a BLE client, scanning for other BLE servers
 * - Connect to found servers and interact with their services
 * - Handle  outgoing connections
 *
 * You can test this example by uploading it to two ESP32 boards.
 *
 * Author: lucasssvaz
 * Based on Arduino BLE examples
 */

/*
TODO: Use touch pads for toggle and wake up from deep sleep
https://randomnerdtutorials.com/esp32-touch-pins-arduino-ide/
https://randomnerdtutorials.com/esp32-touch-wake-up-deep-sleep/
*/

#include <BLEDevice.h>
#include <BLEUtils.h>
#include <BLEServer.h>
#include <BLEClient.h>
#include <BLEScan.h>

// For deep sleep:
#include "driver/rtc_io.h"
#define BUTTON_PIN_BITMASK(GPIO) (1ULL << GPIO)  // 2 ^ GPIO_NUMBER in hex
#define USE_EXT0_WAKEUP          1               // 1 = EXT0 wakeup, 0 = EXT1 wakeup
#define WAKEUP_GPIO              GPIO_NUM_0 //GPIO_NUM_33     // Only RTC IO are allowed - ESP32 Pin example

// Built-in button:
const int buttonPin = 0; 
// GPIO18 - A0:
//const int buttonPin = 18; 

// Remember the remote status
bool remoteOn = false;

// Timer for when to go to sleep
static unsigned long lastEvent = 0;

#include <Adafruit_NeoPixel.h>
Adafruit_NeoPixel pixels(1, PIN_NEOPIXEL, NEO_GRB + NEO_KHZ800);

// Server-side definitions
#define SERVER_SERVICE_UUID        "4fafc201-1fb5-459e-8fcc-c5c9c331914b"
#define SERVER_CHARACTERISTIC_UUID "beb5483e-36e1-4688-b7f5-ea07361b26a8"

// Client-side definitions (looking for the same service)
static BLEUUID clientServiceUUID(SERVER_SERVICE_UUID);
static BLEUUID clientCharUUID(SERVER_CHARACTERISTIC_UUID);

// Client objects
static boolean doConnect = false;
static boolean clientConnected = false;
static BLERemoteCharacteristic *pRemoteCharacteristic;
static BLEAdvertisedDevice *targetDevice;
static BLEClient *pClient = nullptr;
BLEScan *pBLEScan = nullptr;

// Client callbacks
class ClientCallbacks : public BLEClientCallbacks {
  void onConnect(BLEClient *pClient) {
    Serial.println("Client: Connected to server");
    clientConnected = true;
  }

  void onDisconnect(BLEClient *pClient) {
    Serial.println("Client: Disconnected from server");
    clientConnected = false;
  }
};

// Client notification callback
static void notifyCallback(BLERemoteCharacteristic *pBLERemoteCharacteristic, uint8_t *pData, size_t length, bool isNotify) {
  Serial.println("Client: Notify callback for characteristic ");
  String value = (char *) pData;
  updateRemoteState(value);
  lastEvent = millis();
}

// Scan callbacks
class AdvertisedDeviceCallbacks : public BLEAdvertisedDeviceCallbacks {
  void onResult(BLEAdvertisedDevice advertisedDevice) {
    Serial.print("Client: Found device: ");
    Serial.println(advertisedDevice.toString().c_str());

    // Check if this device has our target service
    if (advertisedDevice.haveServiceUUID() && advertisedDevice.isAdvertisingService(clientServiceUUID) && !clientConnected) {

      Serial.println("Client: Found target service, attempting connection...");
      BLEDevice::getScan()->stop();
      targetDevice = new BLEAdvertisedDevice(advertisedDevice);
      doConnect = true;
    }
  }
};

bool connectToServer() {
  Serial.print("Client: Forming connection to ");
  Serial.println(targetDevice->getAddress().toString().c_str());

  // Create client if it doesn't exist, otherwise reuse existing one
  if (pClient == nullptr) {
    pClient = BLEDevice::createClient();
    pClient->setClientCallbacks(new ClientCallbacks());
    Serial.println("Client: Created new client");
  } else {
    Serial.println("Client: Reusing existing client");
  }

  if (!pClient->connect(targetDevice)) {
    Serial.println("Client: Failed to connect");
    return false;
  }

  Serial.println("Client: Connected to server");
  pClient->setMTU(517);  // Request maximum MTU

  // Get the service
  BLERemoteService *pRemoteService = pClient->getService(clientServiceUUID);
  if (pRemoteService == nullptr) {
    Serial.print("Client: Failed to find service UUID: ");
    Serial.println(clientServiceUUID.toString().c_str());
    pClient->disconnect();
    return false;
  }
  Serial.println("Client: Found service");

  // Get the characteristic
  pRemoteCharacteristic = pRemoteService->getCharacteristic(clientCharUUID);
  if (pRemoteCharacteristic == nullptr) {
    Serial.print("Client: Failed to find characteristic UUID: ");
    Serial.println(clientCharUUID.toString().c_str());
    pClient->disconnect();
    return false;
  }
  Serial.println("Client: Found characteristic");

  // Read the initial value
  if (pRemoteCharacteristic->canRead()) {
    String value = pRemoteCharacteristic->readValue();
    updateRemoteState(value);
  }

  // Register for notifications if available
  if (pRemoteCharacteristic->canNotify()) {
    pRemoteCharacteristic->registerForNotify(notifyCallback);
    Serial.println("Client: Registered for notifications");
  }

  return true;
}

void updateRemoteState(String value)
{
  Serial.print("Client: Remote value: ");
  Serial.println(value.c_str());

  if (value == "ON ") {
    Serial.println("The remote is on");
    remoteOn = true;
    pixels.fill(0xFF0000);
    pixels.show();
  } else {
    Serial.println("The remote is off");
    remoteOn = false;
    pixels.fill(0x000000);
    pixels.show();
  }
}

void setupClient() {
  Serial.println("Setting up BLE Client...");

  // Create scanner
  pBLEScan = BLEDevice::getScan();
  pBLEScan->setAdvertisedDeviceCallbacks(new AdvertisedDeviceCallbacks());
  pBLEScan->setActiveScan(true);
  pBLEScan->setInterval(100);
  pBLEScan->setWindow(99);

  Serial.println("Client: Scanner configured");
}

void print_wakeup_reason() {
  esp_sleep_wakeup_cause_t wakeup_reason;

  wakeup_reason = esp_sleep_get_wakeup_cause();

  switch (wakeup_reason) {
    case ESP_SLEEP_WAKEUP_EXT0:     Serial.println("Wakeup caused by external signal using RTC_IO"); break;
    case ESP_SLEEP_WAKEUP_EXT1:     Serial.println("Wakeup caused by external signal using RTC_CNTL"); break;
    case ESP_SLEEP_WAKEUP_TIMER:    Serial.println("Wakeup caused by timer"); break;
    case ESP_SLEEP_WAKEUP_TOUCHPAD: Serial.println("Wakeup caused by touchpad"); break;
    case ESP_SLEEP_WAKEUP_ULP:      Serial.println("Wakeup caused by ULP program"); break;
    default:                        Serial.printf("Wakeup was not caused by deep sleep: %d\n", wakeup_reason); break;
  }
}

void setup() {
  Serial.begin(115200);
  delay(1000);

  //Print the wakeup reason for ESP32
  print_wakeup_reason();

  Serial.println("Starting BLE remote control Client...");

  pinMode(buttonPin, INPUT_PULLUP);

  // Configure deep sleep to wake up on button press
#if USE_EXT0_WAKEUP
  esp_sleep_enable_ext0_wakeup(WAKEUP_GPIO, 0);  //1 = High, 0 = Low
  // Configure pullup/downs via RTCIO to tie wakeup pins to inactive level during deepsleep.
  // EXT0 resides in the same power domain (RTC_PERIPH) as the RTC IO pullup/downs.
  // No need to keep that power domain explicitly, unlike EXT1.
  rtc_gpio_pullup_en(WAKEUP_GPIO);
  rtc_gpio_pulldown_dis(WAKEUP_GPIO);
#endif

  // Initialize BLE device with a name
  BLEDevice::init("ESP32-Coexistence");

  // Setup both server and client
  //setupServer();
  setupClient();

#if defined(NEOPIXEL_POWER)
  // If this board has a power control pin, we must set it to output and high
  // in order to enable the NeoPixels. We put this in an #if defined so it can
  // be reused for other boards without compilation errors
  pinMode(NEOPIXEL_POWER, OUTPUT);
  digitalWrite(NEOPIXEL_POWER, HIGH);
#endif

  pixels.begin(); // INITIALIZE NeoPixel strip object (REQUIRED)
  pixels.setBrightness(20); // not so bright

  pixels.fill(0xFFFF00); // Start with yellow = unknown state
  pixels.show();

  // Start initial scan
  pBLEScan->start(10, false);  // Scan for 10 seconds, don't repeat

  lastEvent = millis();

  Serial.println("Setup complete. Device is scanning as client.");
}

void loop() {
  static unsigned long lastServerUpdate = 0;
  static unsigned long lastClientWrite = 0;
  static unsigned long lastScanStart = 0;
  unsigned long currentTime = millis();

  // Handle client connection attempts
  if (doConnect && !clientConnected) {
    if (connectToServer()) {
      Serial.println("Client: Successfully connected to remote server");
    } else {
      Serial.println("Client: Failed to connect, will retry scanning");
      // Restart scanning after failed connection
      pBLEScan->start(10, false);
    }
    doConnect = false;
  }

  // Write to remote characteristic if connected as client and button pressed
  int buttonState = digitalRead(buttonPin);
  if (clientConnected && pRemoteCharacteristic && buttonState == LOW) {
    remoteOn = !remoteOn;
    lastEvent = millis();

    if (pRemoteCharacteristic->canWrite()) {
      String clientValue = remoteOn ? "ON " : "OFF";
      pRemoteCharacteristic->writeValue(clientValue.c_str(), clientValue.length());
      Serial.print("Client: Wrote to remote characteristic: ");
      Serial.println(clientValue);
      
      if (remoteOn) {
        pixels.fill(0xFF0000); 
        pixels.show();
      } else {
        pixels.fill(0x000000); 
        pixels.show();
      }

      delay(100);
      lastClientWrite = currentTime;
    }
  }

  // Restart scanning periodically if not connected
  if (!clientConnected && currentTime - lastScanStart > 15000) {  // Every 15 seconds
    Serial.println("Client: Restarting scan...");
    pBLEScan->start(10, false);
    lastScanStart = currentTime;
  }

  // If nothing happens, go to deep sleep
  if (currentTime - lastEvent > 30000)
  {
    Serial.println("Going to sleep now");
    esp_deep_sleep_start();
  }

  delay(100);
}
