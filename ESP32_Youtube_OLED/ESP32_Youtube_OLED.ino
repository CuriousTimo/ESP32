/************************************************************************************************************
 *
 *  Copyright (C) 2017 Timo Sariwating
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
This sketch get Youtube Stats and display it on an OLED.
- It uses an ESP32 with i2c OLED Display

/************************************************************************************************************/

// Includes
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include "SSD1306.h"
#include <YoutubeApi.h>

// Wifi
const char* ssid     = "SSID";
const char* password = "PASSWORD";
WiFiClientSecure client;

// Oled Display
SSD1306  display(0x3c, 5, 4);

// YouTube
#define API_KEY "GOOGLE API_KEY"  // your google apps API Token
#define CHANNEL_ID "YOUTUBE CHANNEL_ID" // makes up the url of channel
YoutubeApi api(API_KEY, client);

// Global Variables
int api_mtbs = 60000; //mean time between api requests
long api_lasttime;    //last time api request has been done
long subscriberCount, viewCount, commentCount, videoCount, subs = 0;

/******************************************************************************/
/*          Setup                                                             */
/******************************************************************************/
void setup() {
    Serial.begin(115200);
    delay(10);

    // Start the OLED Display
    display.init();
    display.setFont(ArialMT_Plain_24);
    display.flipScreenVertically();

    display.setColor(WHITE);
    display.setTextAlignment(TEXT_ALIGN_CENTER);
    display.drawString(64, 20, "YouTube");
    
    display.display();

    // Start connecting to a WiFi network
    Serial.println();
    Serial.println();
    Serial.print(F("Connecting to "));
    Serial.println(ssid);

    delay(100);

    WiFi.begin(ssid, password);

    while (WiFi.status() != WL_CONNECTED) {
        delay(500);
        Serial.print(".");
    }

    Serial.println(F(""));
    Serial.println(F("WiFi connected"));
    Serial.println(F("IP address: "));
    Serial.println(WiFi.localIP());

    getYoutubeData();
    
} //Setup


/******************************************************************************/
/*          Program Loop                                                      */
/******************************************************************************/
void loop() {

  if (millis() > api_lasttime + api_mtbs)  {
    getYoutubeData();
    api_lasttime = millis();
  }

  drawOLED();

} //Loop

/******************************************************************************/
/*          Get Youtube Data                                                  */
/******************************************************************************/
void getYoutubeData() {

    if(api.getChannelStatistics(CHANNEL_ID))
    {
      subscriberCount = api.channelStats.subscriberCount;
      viewCount = api.channelStats.viewCount;
      commentCount = api.channelStats.commentCount;
      videoCount = api.channelStats.videoCount;
      
      Serial.println(F("---------Stats---------"));
      Serial.print(F("Subscriber Count: "));
      Serial.println(subscriberCount);
      Serial.print(F("View Count: "));
      Serial.println(viewCount);
      Serial.print(F("Comment Count: "));
      Serial.println(commentCount);
      Serial.print(F("Video Count: "));
      Serial.println(videoCount);
      // Probably not needed :)
      //Serial.print("hiddenSubscriberCount: ");
      //Serial.println(api.channelStats.hiddenSubscriberCount);
      Serial.println("------------------------");
    }
} //getYoutubeData

/******************************************************************************/
/*          Draw OLED                                                         */
/******************************************************************************/
void drawOLED(){

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
      display.drawString( 31, 14, "Subscribers");
      display.drawString( 95, 14, "View Count");

      display.setTextAlignment(TEXT_ALIGN_CENTER);
      
      if (subscriberCount > 9999) {
        display.setFont(ArialMT_Plain_16);
        display.drawString( 31, 35, String(subscriberCount));
      } else {
        display.setFont(ArialMT_Plain_24);
        display.drawString( 31, 30, String(subscriberCount));
      }

      if (viewCount > 9999) {
        display.setFont(ArialMT_Plain_16);
        display.drawString( 95, 35, String(viewCount));          
      } else {
        display.setFont(ArialMT_Plain_24);
        display.drawString( 95, 30, String(viewCount));   
      }
      
      display.display();
} //drawOLED()
