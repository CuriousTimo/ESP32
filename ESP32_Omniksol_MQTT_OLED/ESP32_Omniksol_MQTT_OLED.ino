#include <WiFi.h>
#include <PubSubClient.h>
#include "SSD1306.h"
#include <ArduinoJson.h>

const String sketchName = "ESP32 Omniksol-MQTT Gateway";

// Wifi
WiFiClient wifi_client;
const char* ssid     = "ssid";
const char* password = "password";

// MQTT
PubSubClient mqtt(wifi_client);
const char* mqtt_server = "192.168.60.1";
const String base_topic = "iot/sariwating/";
long lastReconnectAttempt = 0;

// OmnikSol
const long omniksol_serial_number = 602044907;
IPAddress omniksol_ip(192, 168, 60, 30);
uint16_t omniksol_port = 8899;
uint16_t max_omniksol_power = 1500;
char magicMessage[] = {0x68, 0x02, 0x40, 0x30, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x16};

// Oled Display
SSD1306  display(0x3c, 5, 4);
uint8_t screen = 0;

unsigned long omniksol_update, screen_update, stats_update;

// TypeDef
typedef struct {
  char ID[20];
  uint16_t PVVoltageDC; // divide by 10
  uint16_t PVCurrentDC; // divide by 10
  uint16_t VoltageAC;   // divide by 10
  uint16_t PowerAC;     // don't divide
  uint16_t CurrentAC;   // divide 10
  uint16_t FrequencyAC; // divide by 100
  uint16_t Temperature; // divide by 10
  uint16_t PowerToday;  // divide by 100
  uint16_t TotalPower;  // divide by 10
  uint16_t TotalHours;
}Omniksol;
Omniksol omniksol;

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
// Log Onmisol stats to Serial Port
//--------------------------------------------------------------------------------------------
void logSerial() {
  
    Serial.println(F("-----------------------------------------------------------------------"));
    Serial.print(F("ID Inverter: "));
    Serial.println(omniksol.ID);
    
    Serial.print(F("PV Voltage: "));
    Serial.print(float (omniksol.PVVoltageDC) / 10);
    Serial.println(F(" V"));
    
    Serial.print(F("PV Current: "));
    Serial.print(float (omniksol.PVCurrentDC) / 10);
    Serial.println(F(" A"));
    
    Serial.print(F("AC Voltage: "));
    Serial.print(float (omniksol.VoltageAC) / 10);
    Serial.println(F(" V"));
  
    Serial.print(F("AC Power: "));
    Serial.print(omniksol.PowerAC);
    Serial.println(F(" W"));
  
    Serial.print(F("AC Current: "));
    Serial.print(float (omniksol.CurrentAC) / 10);
    Serial.println(F(" A"));
  
    Serial.print(F("AC Frequency: "));
    Serial.print(float (omniksol.FrequencyAC) / 100);
    Serial.println(F(" Hertz"));
    
    Serial.print(F("Inverter Temperature: "));
    Serial.print(float (omniksol.Temperature) / 10);
    Serial.println(F(" °C"));

    Serial.print(F("Inverter Power Today: "));
    Serial.print(float (omniksol.PowerToday) / 100);
    Serial.println(F(" Wh"));

    Serial.print(F("Inverter Total Power: "));
    Serial.print(float (omniksol.TotalPower) / 10);
    Serial.println(F(" kWh"));

    Serial.print(F("Inverter Total Hours: "));
    Serial.print(omniksol.TotalHours);
    Serial.println(F(" Hrs"));
    
    Serial.println(F("-----------------------------------------------------------------------"));
} // End of logSerial

/******************************************************************************/
/*          Draw OLED                                                         */
/******************************************************************************/
void drawOLED() {

      display.clear();
      display.setColor(WHITE);
      display.setTextAlignment(TEXT_ALIGN_CENTER);
      display.setFont(ArialMT_Plain_10);
      display.drawString( 64, 0, "Youtube Curious T!mo");
      display.drawLine(   0, 12, 127, 12);
      display.drawLine(   0, 63, 127, 63);
      display.drawLine(   0, 12,   0, 63);
      display.drawLine( 127, 12, 127, 63);
      display.drawLine(  63, 12,  63, 63);
      
      display.setFont(ArialMT_Plain_10);
      display.setTextAlignment(TEXT_ALIGN_CENTER);

      switch (screen) {
        case 0:
          display.drawString( 31, 14, "Temperature");
          float temp;
          temp = float (omniksol.Temperature) / 10;
          char temperature[4];
          dtostrf(temp, 2, 1, temperature );
          display.setFont(ArialMT_Plain_24);
          display.drawString( 31, 35, String(temperature));
    
          display.setFont(ArialMT_Plain_10);
          display.drawString( 95, 14, "Power");
          display.setFont(ArialMT_Plain_24);
          display.drawString( 95, 35, String(omniksol.PowerAC));   
          break;
        case 1:
          display.drawString( 31, 14, "PV Current");
          float amp;
          amp = float (omniksol.PVCurrentDC) / 10;
          char current[4];
          dtostrf(amp, 2, 1, current );
          display.setFont(ArialMT_Plain_24);
          display.drawString( 31, 35, String(current));
    
          display.setFont(ArialMT_Plain_10);
          display.drawString( 95, 14, "PV Voltage");
          float volt = float (omniksol.PVVoltageDC) / 10;
          char voltage[4];
          dtostrf(volt, 2, 1, voltage );
          display.setFont(ArialMT_Plain_24);
          display.drawString( 95, 35, String(voltage));   
          break;
      }
         
      display.display();
            
} // End of drawOLED()

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
}

//--------------------------------------------------------------------------------------------
// MQTT reconnect
//--------------------------------------------------------------------------------------------
boolean reconnect() {
  if (mqtt.connect("ESP32Client", (base_topic + "esp32" + F("/$online")).c_str(), 1, true, "false")) {
    Serial.println(F("MQTT Connected"));
    
    // Once connected, publish an announcement...
    sendMqttStats();
    
    // ... and resubscribe
    mqtt.subscribe((base_topic + "esp32" + F("/$set")).c_str());
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
// Send Omniksol stats to MQTT
//--------------------------------------------------------------------------------------------
void sendOmniksolData() {

  DynamicJsonBuffer  dataBuffer;
  JsonObject& dataJson = dataBuffer.createObject();

  dataJson[F("ID")]              = omniksol.ID;
  dataJson[F("PowerAC")]         = omniksol.PowerAC;
  dataJson[F("VoltageAC")]       = float (omniksol.VoltageAC) / 10;
  dataJson[F("CurrentAC")]       = float (omniksol.CurrentAC) / 10;
  dataJson[F("FrequencyAC")]     = float (omniksol.FrequencyAC) / 100;
  dataJson[F("PVVoltageDC")]     = float (omniksol.PVVoltageDC) / 10;
  dataJson[F("PVCurrentDC")]     = float (omniksol.PVCurrentDC) / 10;
  dataJson[F("PowerToday")]      = float (omniksol.PowerToday) / 100;
  dataJson[F("TotalPower")]      = float (omniksol.TotalPower) / 10;
  dataJson[F("TotalHours")]      = omniksol.TotalHours;
  dataJson[F("Temperature")]     = float (omniksol.Temperature) / 10;

  mqttPublish( (base_topic + "esp32" + F("/json")).c_str(), dataJson);
} // End of sendOmniksolStats

//--------------------------------------------------------------------------------------------
// Calculate the Magic Message that needs to be send to the Omniksol
//--------------------------------------------------------------------------------------------
void calcMagicMessage() {
  /*
  generate the magic message
  first 4 bytes are fixed x68 x02 x40 x30
  next 8 bytes are the reversed serial number twice(hex)
  next 2 bytes are fixed x01 x00
  next byte is a checksum (2x each binary number form the serial number + 115)
  last byte is fixed x16
  */

  int checksum = 0;
  
  for (uint8_t i=0; i<4; i++) {
    magicMessage[4+i] = magicMessage[8+i] = ((omniksol_serial_number>>(8*i))&0xff);
    checksum += magicMessage[4+i];
  }
  checksum *= 2;
  checksum += 115;
  checksum &= 0xff;
  magicMessage[14] = checksum;

  Serial.print(F("Omniksol Magic Message: "));

  for (uint8_t i=0; i<13; i++) {
    Serial.print(magicMessage[i],HEX);
    Serial.print(" ");
  }
  Serial.println();
} // End of calcMagicMessage

//--------------------------------------------------------------------------------------------
// Connect to Omniksol Inverter and get Stats
//--------------------------------------------------------------------------------------------
void getOmniksolData() {

  Serial.println(F("Connecting to Omniksol Inverter....."));

  WiFiClient omniksol_client;
  if (!omniksol_client.connect(omniksol_ip, omniksol_port)) {
    Serial.println(F("Connection Failed"));
    return;
  }

  if(omniksol_client.connected()) {

    Serial.println(F("Getting Omniksol stats....."));

    // Send the Magic String to the Omniksol
    omniksol_client.print(magicMessage);
  
    // Wait 5 seconds for a reply 
    unsigned long timeout = millis();
    while (omniksol_client.available() == 0) {
      if (millis() - timeout > 5000) {
        Serial.println(F(">>> Omniksol Timeout !"));
        omniksol_client.stop();
        return;
      }
    }
  
    // Read all data
    uint16_t dataSize = omniksol_client.available();
  
    char server_reply[dataSize + 1];
  
    if(omniksol_client.available() > 0 && dataSize == 99) {
      for(uint16_t i = 0; i < dataSize - 1; i++) {
        server_reply[i] = omniksol_client.read();
        Serial.print(server_reply[i], HEX);
        Serial.print(" ");
      }
    }
  
    Serial.println();
    Serial.println(F("Closing Connection"));
  
    omnikFillStruct(server_reply);
  } else {
    Serial.println(F("Omniksol Inverter not Online!"));
  }
  
} // End of getOmniksolStats

//--------------------------------------------------------------------------------------------
// Load the Received data into the struct Array
//--------------------------------------------------------------------------------------------
void omnikFillStruct(char *server_reply) {

  // First Check for valid data (If the power is higher than max_omniksol_power it's invalid)
  if ( (server_reply[60] + (server_reply[59] * 256)) <= max_omniksol_power ) {

    // Get Omniksol Inverter ID
    strncpy(omniksol.ID, &server_reply[15], 16);
    omniksol.ID[16] = 0;
  
    // Get Omniksol Inverter DC Voltage
    omniksol.PVVoltageDC = server_reply[34] + (server_reply[33] * 256);
  
    // Get Omniksol Inverter DC Current
    omniksol.PVCurrentDC = server_reply[40] + (server_reply[39] * 256);
  
    // Get Omniksol Inverter AC Voltage
    omniksol.VoltageAC = server_reply[52] + (server_reply[51] * 256);
  
    // Get Omniksol Inverter AC Power
    omniksol.PowerAC = server_reply[60] + (server_reply[59] * 256);
  
    // Get Omniksol Inverter AC Current
    omniksol.CurrentAC = server_reply[46] + (server_reply[45] * 256);
  
    // Get Omniksol Inverter AC Frequency
    omniksol.FrequencyAC = server_reply[58] + (server_reply[57] * 256);
  
    // Get Omniksol Inverter Temperature
    omniksol.Temperature = server_reply[32] + (server_reply[31] * 256);

    // Get Omniksol Inverter Power Today
    omniksol.PowerToday = server_reply[70] + (server_reply[69] * 256);

    // Get Omniksol Inverter Total Power
    omniksol.TotalPower = server_reply[74] + (server_reply[73] * 256) + (server_reply[72] * 65536) + (server_reply[71] * 16777216);

    // Get Omniksol Inverter Total Hours
    omniksol.TotalHours = server_reply[78] + (server_reply[77] * 256) + (server_reply[76] * 65536) + (server_reply[75] * 16777216);

    // Log to Serial Port
    logSerial();
    // Send stats to MQTT
    sendOmniksolData();

  } else {
    Serial.println(F("Invalid Omniksol Data!"));
  }

} // End of omnikFillStruct

//--------------------------------------------------------------------------------------------
// Setup Routine
//--------------------------------------------------------------------------------------------
void setup() {

  // Setup Serial Port aut 115200 Baud
  startSerial(115200);

  // Start the OLED Display
  display.init();
  display.setFont(ArialMT_Plain_24);
  //display.flipScreenVertically();                 // this is to flip the screen 180 degrees

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

  // Calculate MagicMessage
  calcMagicMessage();

  // Get Omniksol Stats
  getOmniksolData();

  lastReconnectAttempt = 0;
}

//--------------------------------------------------------------------------------------------
// Main Program Loop
//--------------------------------------------------------------------------------------------
void loop() {

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
  // Every 5 Seconds
  if ((millis()-screen_update)>5000) { // 5 Seconds
    screen++;
    if (screen > 1) {
      screen = 0;
    }
    screen_update = millis();
  }

  // Every 10 Seconds
  if ((millis()-stats_update)>10000) { // 10 Seconds
    stats_update = millis();
    sendMqttStats();
  }
  
  // Every 15 Seconds
  if ((millis()-omniksol_update)>15000) { // 15 Seconds
    omniksol_update = millis();
    getOmniksolData();
  }

  drawOLED();
} // End of Loop
