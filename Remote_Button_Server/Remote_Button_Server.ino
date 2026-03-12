/*
 * BLE Server Coexistence Example
 *
 * This example demonstrates how to run both BLE client and server
 * functionality on the same ESP32 device simultaneously.
 *
 * The device will:
 * - Act as a BLE server, advertising a service with a characteristic
 * - Handle incoming connections
 *
 * You can test this example by uploading it to two ESP32 boards.
 *
 * Author: lucasssvaz
 * Based on Arduino BLE examples
 */

#include <BLEDevice.h>
#include <BLEUtils.h>
#include <BLEServer.h>
#include <BLEClient.h>
#include <BLEScan.h>

#include <Adafruit_NeoPixel.h>
Adafruit_NeoPixel pixels(1, PIN_NEOPIXEL, NEO_GRB + NEO_KHZ800);

// Server-side definitions
#define SERVER_SERVICE_UUID        "4fafc201-1fb5-459e-8fcc-c5c9c331914b"
#define SERVER_CHARACTERISTIC_UUID "beb5483e-36e1-4688-b7f5-ea07361b26a8"

// Client-side definitions (looking for the same service)
static BLEUUID clientServiceUUID(SERVER_SERVICE_UUID);
static BLEUUID clientCharUUID(SERVER_CHARACTERISTIC_UUID);

// Server objects
BLEServer *pServer = nullptr;
BLECharacteristic *pServerCharacteristic = nullptr;

// Server callbacks
class ServerCallbacks : public BLEServerCallbacks {
  void onConnect(BLEServer *pServer) {
    Serial.println("Server: Client connected");
  }

  void onDisconnect(BLEServer *pServer) {
    Serial.println("Server: Client disconnected");
    // Restart advertising
    BLEDevice::startAdvertising();
  }
};

// Characteristic callbacks for server
class CharacteristicCallbacks : public BLECharacteristicCallbacks {
  void onWrite(BLECharacteristic *pCharacteristic) {
    String value = pCharacteristic->getValue();
    Serial.print("Server: Characteristic written, value: ");
    Serial.println(value.c_str());

    if (value == "ON ")
    {
      Serial.print("We are on");
      pixels.fill(0xFF0000);
      pixels.show();
    }
    else
    {
      Serial.print("We are off");
      pixels.fill(0x000000);
      pixels.show();
    }
  }

  void onRead(BLECharacteristic *pCharacteristic) {
    Serial.println("Server: Characteristic read");
  }
};

void setupServer() {
  Serial.println("Setting up BLE Server...");

  // Create server
  pServer = BLEDevice::createServer();
  pServer->setCallbacks(new ServerCallbacks());

  // Create service
  BLEService *pService = pServer->createService(SERVER_SERVICE_UUID);

  // Create characteristic
  pServerCharacteristic = pService->createCharacteristic(
    SERVER_CHARACTERISTIC_UUID, BLECharacteristic::PROPERTY_READ | BLECharacteristic::PROPERTY_WRITE | BLECharacteristic::PROPERTY_NOTIFY
  );

  pServerCharacteristic->setCallbacks(new CharacteristicCallbacks());
  pServerCharacteristic->setValue("Hello from Coexistence Server");

  // Start service
  pService->start();

  // Start advertising
  BLEAdvertising *pAdvertising = BLEDevice::getAdvertising();
  pAdvertising->addServiceUUID(SERVER_SERVICE_UUID);
  pAdvertising->setScanResponse(true);
  pAdvertising->setMinPreferred(0x06);
  pAdvertising->setMinPreferred(0x12);

  BLEDevice::startAdvertising();

  Serial.println("Server: Advertising started");
}

void setup() {
  Serial.begin(115200);
  Serial.println("Starting BLE Server Coexistence Example...");

  // Initialize BLE device with a name
  BLEDevice::init("ESP32-Coexistence");

  // Setup server 
  setupServer();

#if defined(NEOPIXEL_POWER)
  // If this board has a power control pin, we must set it to output and high
  // in order to enable the NeoPixels. We put this in an #if defined so it can
  // be reused for other boards without compilation errors
  pinMode(NEOPIXEL_POWER, OUTPUT);
  digitalWrite(NEOPIXEL_POWER, HIGH);
#endif

  pixels.begin(); // INITIALIZE NeoPixel strip object (REQUIRED)
  pixels.setBrightness(20); // not so bright

  Serial.println("Setup complete. Device is advertising as server.");
}

void loop() {
  static unsigned long lastServerUpdate = 0;
  static unsigned long lastClientWrite = 0;
  static unsigned long lastScanStart = 0;
  unsigned long currentTime = millis();

  // Update server characteristic periodically
  if (currentTime - lastServerUpdate > 1000) {  // Every 1 seconds
    if (pServerCharacteristic) {
      String value = pServerCharacteristic->getValue();
      Serial.print("Server: Current Characteristic value: ");
      Serial.println(value.c_str());
      // const char *buf = value.c_str();
      // for (; *buf !=  '\0'; buf++)
      //   Serial.println(*buf);
      // Serial.println("End");

      // if (value == "ON ")
      // {
      //   Serial.print("We are on");
      //   pixels.fill(0xFF0000);
      //   pixels.show();
      // }
      // else
      // {
      //   Serial.print("We are off");
      //   pixels.fill(0x000000);
      //   pixels.show();
      // }
    }

    lastServerUpdate = currentTime;
  }

  delay(100);
}
