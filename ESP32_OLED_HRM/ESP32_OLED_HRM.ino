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
This sketch gets Heart Rate from an BLE HRM monitor and display it on an OLED.
- It uses an ESP32 with i2c OLED Display
/************************************************************************************************************/
#include <WiFi.h>
#include <PubSubClient.h>
#include "BLEDevice.h"
#include "SSD1306.h"
#include <ArduinoJson.h>

const String sketchName = "ESP32 HRM BLE Client";

// BLE
// The remote HRM service we wish to connect to.
static  BLEUUID serviceUUID(BLEUUID((uint16_t)0x180D));
// The HRM characteristic of the remote service we are interested in.
static  BLEUUID    charUUID(BLEUUID((uint16_t)0x2A37));

static BLEAddress *pServerAddress;
static boolean doConnect = false;
static boolean connected = false;
static boolean notification = false;
static BLERemoteCharacteristic* pRemoteCharacteristic;

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

unsigned long screen_update, stats_update;

// TypeDef
typedef struct {
  char ID[20];
  uint16_t HRM;
}HRM;
HRM hrm;

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
      display.drawString( 64, 20, String(hrm.HRM) + " bpm");

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
// Send HRM stats to MQTT
//--------------------------------------------------------------------------------------------
void sendHRMData() {

  DynamicJsonBuffer  dataBuffer;
  JsonObject& dataJson = dataBuffer.createObject();

  dataJson[F("HRM")]             = hrm.HRM;

  mqttPublish( (base_topic + "esp32" + F("/json")).c_str(), dataJson);
} // End of sendHRMData

//--------------------------------------------------------------------------------------------
// BLE notifyCallback
//--------------------------------------------------------------------------------------------
static void notifyCallback( BLERemoteCharacteristic* pBLERemoteCharacteristic, uint8_t* pData, size_t length, bool isNotify) {

  if (bitRead(pData[0], 0) == 1) {
    Serial.println(F("16bit HeartRate Detected"));
  } else {
    Serial.println(F("8bit HeartRate Detected"));
  }

  if (length == 2) {
    hrm.HRM = pData[1];
    Serial.print("Heart Rate ");
    Serial.print(hrm.HRM, DEC);
    Serial.println("bpm");

    sendHRMData();
  }
}

//--------------------------------------------------------------------------------------------
//  Connect to BLE HRM
//--------------------------------------------------------------------------------------------
bool connectToServer(BLEAddress pAddress) {
    Serial.print(F("Forming a connection to "));
    Serial.println(pAddress.toString().c_str());

    BLEClient*  pClient  = BLEDevice::createClient();
    Serial.println(F(" - Created client"));
    
    // Connect to the HRM BLE Server.
    pClient->connect(pAddress);
    Serial.println(F(" - Connected to server"));

    // Obtain a reference to the service we are after in the remote BLE server.
    BLERemoteService* pRemoteService = pClient->getService(serviceUUID);
    if (pRemoteService == nullptr) {
      Serial.print(F("Failed to find our service UUID: "));
      Serial.println(serviceUUID.toString().c_str());
      return false;
    }
    Serial.println(F(" - Found our service"));


    // Obtain a reference to the characteristic in the service of the remote BLE server.
    pRemoteCharacteristic = pRemoteService->getCharacteristic(charUUID);
    if (pRemoteCharacteristic == nullptr) {
      Serial.print(F("Failed to find our characteristic UUID: "));
      Serial.println(charUUID.toString().c_str());
      return false;
    }
    Serial.println(F(" - Found our characteristic"));

    // Register for Notify
    pRemoteCharacteristic->registerForNotify(notifyCallback);
}

//--------------------------------------------------------------------------------------------
// Scan for BLE servers and find the first one that advertises the service we are looking for.
//--------------------------------------------------------------------------------------------
class MyAdvertisedDeviceCallbacks: public BLEAdvertisedDeviceCallbacks {
 /**
   * Called for each advertising BLE server.
   */
  void onResult(BLEAdvertisedDevice advertisedDevice) {
    Serial.print(F("BLE Advertised Device found: "));
    Serial.println(advertisedDevice.toString().c_str());

    // We have found a device, let us now see if it contains the service we are looking for.
    if (advertisedDevice.haveServiceUUID() && advertisedDevice.getServiceUUID().equals(serviceUUID)) {

      // 
      Serial.print(F("Found our device!  address: ")); 
      advertisedDevice.getScan()->stop();

      pServerAddress = new BLEAddress(advertisedDevice.getAddress());
      doConnect = true;

    } // Found our server
  } // onResult
}; // MyAdvertisedDeviceCallbacks

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

  Serial.println("");
  Serial.println(F("WiFi connected"));  
  Serial.print(F("IP address: "));
  Serial.println(WiFi.localIP());

  // MQTT
  mqtt.setServer(mqtt_server, 1883);
  mqtt.setCallback(callback);

  lastReconnectAttempt = 0;

  // Start BLE
  BLEDevice::init("");

  // Retrieve a Scanner and set the callback we want to use to be informed when we
  // have detected a new device.  Specify that we want active scanning and start the
  // scan to run for 30 seconds.
  BLEScan* pBLEScan = BLEDevice::getScan();
  pBLEScan->setAdvertisedDeviceCallbacks(new MyAdvertisedDeviceCallbacks());
  pBLEScan->setActiveScan(true);
  pBLEScan->start(30);
} // End of setup.

//--------------------------------------------------------------------------------------------
// Main Program Loop
//--------------------------------------------------------------------------------------------
void loop() {

  // If the flag "doConnect" is true then we have scanned for and found the desired
  // BLE Server with which we wish to connect.  Now we connect to it.  Once we are 
  // connected we set the connected flag to be true.
  if (doConnect == true) {
    if (connectToServer(*pServerAddress)) {
      Serial.println(F("We are now connected to the BLE HRM"));
      connected = true;
    } else {
      Serial.println(F("We have failed to connect to the HRM; there is nothin more we will do."));
    }
    doConnect = false;
  }

  // Turn notification on
  if (connected) {
    if (notification == false) {
      Serial.println(F("Turning Notifocation On"));
      const uint8_t onPacket[] = {0x1, 0x0};
      pRemoteCharacteristic->getDescriptor(BLEUUID((uint16_t)0x2902))->writeValue((uint8_t*)onPacket, 2, true);
      notification = true;
    }
    
  }

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
} // End of loop
