/************************************************************************************************************
 *
 *  Copyright(C) 2017 Timo Sariwating
 *
 *  This program is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 3 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  Lesser General Public License for more details.
 *
 *  You should have received a copy of the GNU Lesser General Public License
 *  along with this program; if not, If not, see <http://www.gnu.org/licenses/>.
 *
 ************************************************************************************************************
DESCRIPTION:
This sketch gets Data from an BLE MiFLora sensor and display it on an OLED.
- It uses an ESP32 with i2c OLED Display
/************************************************************************************************************/
#include <WiFi.h>
#include <PubSubClient.h>
#include "BLEDevice.h"
#include "SSD1306.h"
#include <ArduinoJson.h>

const String sketchName = "ESP32 MiFlora BLE-MQTT Gateway";

// BLE
// The remote BLE service we wish to connect to.
static BLEUUID  serviceUUID("00001204-0000-1000-8000-00805f9b34fb");

// The characteristic of the remote service we are interested in.
static BLEUUID    charUUID1("00001a00-0000-1000-8000-00805f9b34fb");
static BLEUUID    charUUID2("00001a01-0000-1000-8000-00805f9b34fb");
static BLEUUID    charUUID3("00001a02-0000-1000-8000-00805f9b34fb");

static BLEAddress *pServerAddress;
static BLERemoteCharacteristic* pRemoteCharacteristic, *pRC;

static boolean doConnect = false;
static boolean connected = false;

BLERemoteService* pRemoteService;
BLEClient*  pClient;

// Wifi
WiFiClient wifi_client;
const char* ssid     = "SSID";
const char* password = "PASSWORD";

// MQTT
PubSubClient mqtt(wifi_client);
const char* mqtt_server = "192.168.60.1";
const String base_topic = "iot/sariwating/";
long lastReconnectAttempt = 0;

// Oled Display
SSD1306  display(0x3c, 5, 4);

unsigned long screen_update, stats_update, sensor_update;

// TypeDef
typedef struct {
  char      BLEMAC[18];
  char      FW[6];
  uint8_t   Battery;
  float     Temperature;
  uint8_t   Moisture;
  uint8_t   Light;
  uint8_t   Fertility;
}MIFLORA;
MIFLORA miflora;

//--------------------------------------------------------------------------------------------
// Setup the Serial Port and output Sketch name and compile date
//--------------------------------------------------------------------------------------------
void startSerial(uint32_t baud) {
  
  // Setup Serial Port aut 115200 Baud
  Serial.begin(baud);
  
  delay(10);
  
  Serial.println();
  Serial.print(sketchName);
  Serial.print(F(" | Compiled: "));
  Serial.print(__DATE__);
  Serial.print(F("/"));
  Serial.println(__TIME__);
} // End of startSerial

//--------------------------------------------------------------------------------------------
// Draw OLED
//--------------------------------------------------------------------------------------------
void drawOLED(){

      display.clear();
      display.setColor(WHITE);
      display.setTextAlignment(TEXT_ALIGN_CENTER);
      display.setFont(ArialMT_Plain_24);
      display.drawString( 64, 20, String(miflora.Moisture) + " %");

      display.display();
      
} //drawOLED()

//--------------------------------------------------------------------------------------------
// MQTT callback routine
//--------------------------------------------------------------------------------------------
void callback(char* topic, byte* payload, unsigned int length) {
  // handle message arrived
} // End of callback

//--------------------------------------------------------------------------------------------
// MQTT send update
//--------------------------------------------------------------------------------------------
void sendMqttStats() {
    mqtt.publish( (base_topic + "esp32" + F("/$name")).c_str(), sketchName.c_str(), true);
    mqtt.publish( (base_topic + "esp32" + F("/$localip")).c_str(), WiFi.localIP().toString().c_str(), true);
    mqtt.publish( (base_topic + "esp32" + F("/$mac")).c_str(), WiFi.macAddress().c_str(), true);
    mqtt.publish( (base_topic + "esp32" + F("/$arch")).c_str(), "xtensa-esp32", true);
    mqtt.publish( (base_topic + "esp32" + F("/$sdk")).c_str(), String(ARDUINO).c_str(), true);
    mqtt.publish( (base_topic + "esp32" + F("/$fwversion")).c_str(), (String(__DATE__) + "/" + String(__TIME__)).c_str(), true);
    mqtt.publish( (base_topic + "esp32" + F("/$online")).c_str(), "true", true);
} // End of sendMqttStats()

//--------------------------------------------------------------------------------------------
// MQTT reconnect
//--------------------------------------------------------------------------------------------
boolean reconnect() {
  if (mqtt.connect("ESP32Client", (base_topic + "esp32" + F("/$online")).c_str(), 1, true, "false")) {
    Serial.println(F("MQTT Connected"));
    
    // Once connected, publish an announcement...
    sendMqttStats();
    bleScan();
    
    // ... and resubscribe
  }
  return mqtt.connected();
} // End of reconnect

//--------------------------------------------------------------------------------------------
// Publish JSON to MQTT
//--------------------------------------------------------------------------------------------
void mqttPublish(const char* topic, JsonObject& sendJson) {
  
  if (mqtt.connected()) {
    char buffer[sendJson.measureLength() + 1];
    sendJson.printTo(buffer, sizeof(buffer));
    mqtt.publish(topic, buffer, true);
  }
  
} // End of mqttPublish

//--------------------------------------------------------------------------------------------
// Send MiFlora BLE stats to MQTT
//--------------------------------------------------------------------------------------------
void sendBLEData() {

  DynamicJsonBuffer  dataBuffer;
  JsonObject& dataJson = dataBuffer.createObject();

  dataJson[F("BLEMAC")]         = miflora.BLEMAC;
  dataJson[F("FW")]             = miflora.FW;
  dataJson[F("Battery")]        = miflora.Battery;
  dataJson[F("Temperature")]    = miflora.Temperature;
  dataJson[F("Moisture")]       = miflora.Moisture;
  dataJson[F("Light")]          = miflora.Light;
  dataJson[F("Fertility")]      = miflora.Fertility;

  mqttPublish( (base_topic + "esp32" + F("/json")).c_str(), dataJson);
} // End of sendBLEData

//--------------------------------------------------------------------------------------------
//  BLE MiFlora Notify Callback
//--------------------------------------------------------------------------------------------
static void notifyCallback(BLERemoteCharacteristic* pBLERemoteCharacteristic, uint8_t* pData, size_t length, bool isNotify) {
    Serial.print(F("Notify callback for characteristic "));
    Serial.print(pBLERemoteCharacteristic->getUUID().toString().c_str());
    Serial.print(F(" of data length "));
    Serial.println(length);
} // End of notifyCallback

//--------------------------------------------------------------------------------------------
//  Connect to BLE MiFlora Sensor
//--------------------------------------------------------------------------------------------
bool connectToServer(BLEAddress pAddress) {
    Serial.print(F(" - Forming a connection to "));
    String BLEmac = pAddress.toString().c_str();
    BLEmac.toCharArray(miflora.BLEMAC, 18);
    Serial.println(BLEmac);
    
    pClient  = BLEDevice::createClient();
    Serial.println(F(" - Created client"));

    // Connect to the remote BLE Server.
    pClient->connect(pAddress);
    Serial.println(F(" - Connected to server"));

    // Obtain a reference to the service we are after in the remote BLE server.
    pRemoteService = pClient->getService(serviceUUID);
    if (pRemoteService == nullptr) {
        Serial.print(F("Failed to find our service UUID: "));
        Serial.println(serviceUUID.toString().c_str());
        for(;;);
    }
    Serial.println(F(" - Found our service"));

    // Obtain a reference to the control characteristic in the service of the remote BLE server.
    pRemoteCharacteristic = pRemoteService->getCharacteristic(charUUID1);
    if (pRemoteCharacteristic == nullptr) {
      Serial.print("Failed to find our characteristic UUID: ");
      Serial.println(charUUID1.toString().c_str());
      for(;;);
    }
    
    Serial.println(F(" - Setting control characteristic value to 0xA01F"));
    const uint8_t newValue[] = {0xA0, 0x1F};

    // Set the characteristic's value to be the array of bytes.
    pRemoteCharacteristic->writeValue((uint8_t*)newValue, 2, true); // Enable real-time reporting

    return true;
}

//--------------------------------------------------------------------------------------------
// Scan for BLE servers and find the first one that advertises the service we are looking for.
//--------------------------------------------------------------------------------------------
class MyAdvertisedDeviceCallbacks: public BLEAdvertisedDeviceCallbacks {
 /**
   * Called for each advertising BLE server.
   */
  void onResult(BLEAdvertisedDevice advertisedDevice) {
    Serial.print(F(" - BLE Advertised Device found: "));
    Serial.println(advertisedDevice.toString().c_str());

    // We have found a device, let us now see if it contains the service we are looking for.

    if (advertisedDevice.getName() == "Flower care") {
      Serial.print(F(" - Found our device!  address: ")); 
      advertisedDevice.getScan()->stop();

      pServerAddress = new BLEAddress(advertisedDevice.getAddress());
      doConnect = true;
    } // Found our server
  } // onResult
}; // MyAdvertisedDeviceCallbacks

//--------------------------------------------------------------------------------------------
// Do a BLE Scan
//--------------------------------------------------------------------------------------------
void bleScan() {
  
  // Retrieve a Scanner and set the callback we want to use to be informed when we
  // have detected a new device.  Specify that we want active scanning and start the
  // scan to run for 15 seconds.
  BLEScan* pBLEScan = BLEDevice::getScan();
  pBLEScan->setAdvertisedDeviceCallbacks(new MyAdvertisedDeviceCallbacks());
  pBLEScan->setActiveScan(true);
  pBLEScan->start(15);
  
} // End of bleScan()

//--------------------------------------------------------------------------------------------
// Get the BLE data from the Sensor
//--------------------------------------------------------------------------------------------
void getBLEData() {

  // If the flag "doConnect" is true then we have scanned for and found the desired
  // BLE Server with which we wish to connect.  Now we connect to it.  Once we are 
  // connected we set the connected flag to be true.
  if (doConnect == true) {
    if (connectToServer(*pServerAddress)) {
      Serial.println(F(" - We are now connected to the BLE Server."));
      connected = true;
    } else {
      Serial.println(F("We have failed to connect to the server; there is nothin more we will do."));
    }
    doConnect = false;
  }


  // If we are connected to a peer BLE Server, update the characteristic to enable real-time reads
  if (connected) {

    // Find the battery characteristic
    pRemoteCharacteristic = pRemoteService->getCharacteristic(charUUID3);
    if (pRemoteCharacteristic == nullptr) {
      Serial.print(F("Failed to find our characteristic UUID: "));
      Serial.println(charUUID3.toString().c_str());
      for(;;);
    }
    
    delay(100);
    Serial.println(F(" - Found our battery/firmware characteristic"));

    // Read the value of the characteristic.
    std::string batFw = pRemoteCharacteristic->readValue();
    Serial.print(F(" - The characteristic value was: "));
    Serial.print(F("Battery "));
    miflora.Battery = (byte)batFw[0];
    Serial.print(miflora.Battery);
    Serial.print(F("% | Firmware "));
    String fw = String(batFw.substr(2,5).c_str());
    fw.toCharArray(miflora.FW, 6);
    Serial.print(fw);
    Serial.println();

    // Obtain a reference to the sensor characteristic in the service of the remote BLE server.
    pRC = pRemoteService->getCharacteristic(charUUID2);
    if (pRC == nullptr) {
      Serial.print(F("Failed to find our characteristic UUID: "));
      Serial.println(charUUID2.toString().c_str());
      for(;;);
    }
    Serial.println(F(" - Found sensor characteristic"));
    
 // Read the value of the sensor characteristic.
    std::string value = pRC->readValue();
    delay(100);
    Serial.print(F(" - The characteristic value was: "));
    for (int i=0; i<value.length(); i++) {
      Serial.print((byte)value[i], HEX);
      Serial.print(F(" "));
    }
    Serial.println();

    miflora.Temperature = (float)(value[0] + (value[1] * 256)) / 10;
    miflora.Moisture  = value[7];
    miflora.Light     = value[3];
    miflora.Fertility = value[8];

    Serial.print(F(" - Temp: "));
    Serial.print(miflora.Temperature);
    Serial.print(F("C | Moisture: "));
    Serial.print(miflora.Moisture);
    Serial.print(F("% | Light: "));
    Serial.print(miflora.Light );
    Serial.print(F(" Lux | Fertility: "));
    Serial.print(miflora.Fertility);
    Serial.print(F(" us/cm"));
    Serial.println();

  Serial.println(F(" - Disconnecting From Sensor"));
  pClient->disconnect();
  doConnect = false;

  // Send to MQTT
  Serial.println(F(" - Sending to MQTT"));
  sendBLEData();
  }

} // End of getBLEData

//--------------------------------------------------------------------------------------------
// Setup Routine
//--------------------------------------------------------------------------------------------
void setup() {

  // Setup Serial Port aut 115200 Baud
  startSerial(115200);

  // Start the OLED Display
  display.init();
  display.setFont(ArialMT_Plain_24);
  display.flipScreenVertically();                 // this is to flip the screen 180 degrees

  display.setColor(WHITE);
  display.setTextAlignment(TEXT_ALIGN_CENTER);
  display.drawString(64, 20, "YouTube");

  display.display();

  // We will connect to the WiFi network
  Serial.print(F("Connecting to "));
  Serial.println(ssid);
  
  /* Explicitly set the ESP32 to be a WiFi-client, otherwise, it by default,
     would try to act as both a client and an access-point and could cause
     network-issues with your other WiFi-devices on your WiFi-network. */
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);
  
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(F("."));
  }

  Serial.println(F(""));
  Serial.println(F("WiFi connected"));  
  Serial.print(F("IP address: "));
  Serial.println(WiFi.localIP());

  // MQTT
  mqtt.setServer(mqtt_server, 1883);
  mqtt.setCallback(callback);

  lastReconnectAttempt = 0;

  // Start BLE

  BLEDevice::init("");
  
} // End of setup.


//--------------------------------------------------------------------------------------------
// Main Program Loop
//--------------------------------------------------------------------------------------------
void loop() {

  // BLE
  if (doConnect == true) {
    getBLEData();
  }

  // MQTT
  if (!mqtt.connected()) {
    long now = millis();
    if (now - lastReconnectAttempt > 5000) {
      lastReconnectAttempt = now;
      // Attempt to reconnect
      if (reconnect()) {
        lastReconnectAttempt = 0;
      }
    }
  } else {
    // Client connected
    mqtt.loop();
  }

  //-------------------------------------------------------------------------------------------
  // Timed Code
  //-------------------------------------------------------------------------------------------
  // Every 100 Milliseconds
  if ((millis()-screen_update)>100) { // 100 Milliseconds
    drawOLED();
    screen_update = millis();
  }

  // Every 10 Seconds
  if ((millis()-stats_update)>10000) { // 10 Seconds
    stats_update = millis();
    sendMqttStats();
  }

    // Every 30 Seconds
  if ((millis()-sensor_update)>30000) { // 30 Seconds
    sensor_update = millis();
    bleScan();
  }
} // End of loop
