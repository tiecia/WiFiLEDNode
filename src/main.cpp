#include <ESP8266WiFi.h>
#include <FS.h>
#include <WiFiClient.h>
#include <TimeLib.h>
#include <NtpClientLib.h>
#include <ESPAsyncTCP.h>
#include <ESPAsyncWebServer.h>
#include <ESP8266mDNS.h>
#include <Ticker.h>
#include <ArduinoOTA.h>
#include <ArduinoJson.h>
#include <FSWebServerLib.h>
#include <Hash.h>

#include <ArtnetWifi.h>
#include <Arduino.h>
#include <FastLED.h>

#include "main.h"

/**
 * Light Behavior
 * 
 * Green:
 *    On = Recieving Data
 *    Off = No data recieved
 * 
 * Red:
 *    On = Not connected to wifi
 *    Off = Connected to WiFi
 *    Three fast blinks = AP Mode enabled
 */


// LED settings
const int numLeds = 510; // CHANGE FOR YOUR SETUP
const int numberOfChannels = numLeds * 3; // Total number of channels you want to receive (1 led = 3 channels)


CRGB leds[numLeds];

// Art-Net settings
ArtnetWifi artnet;
const int startUniverse = 0; // CHANGE FOR YOUR SETUP most software this is 1, some software send out artnet first universe as 0.

// Check if we got all universes
const int maxUniverses = numberOfChannels / 512 + ((numberOfChannels % 512) ? 1 : 0);
bool universesReceived[maxUniverses];
bool sendFrame = 1;
int previousDataLength = 0;

int lastFrameTime;

int lastDebug;

void indicateDataRecieve(bool state){
  digitalWrite(ARTNET_INDICATION_LIGHT, state);
  digitalWrite(LED_BUILTIN, !state);
}

void initTest()
{
  for (int i = 0 ; i < numLeds ; i++) {
    leds[i] = CRGB(127, 0, 0);
  }
  FastLED.show();
  delay(500);
  for (int i = 0 ; i < numLeds ; i++) {
    leds[i] = CRGB(0, 127, 0);
  }
  FastLED.show();
  delay(500);
  for (int i = 0 ; i < numLeds ; i++) {
    leds[i] = CRGB(0, 0, 127);
  }
  FastLED.show();
  delay(500);
  for (int i = 0 ; i < numLeds ; i++) {
    leds[i] = CRGB(0, 0, 0);
  }
  FastLED.show();
}

void onDmxFrame(uint16_t universe, uint16_t length, uint8_t sequence, uint8_t* data)
{
  lastFrameTime = millis();
  indicateDataRecieve(HIGH);
  // delay(50);


  sendFrame = 1;

  // set brightness of the whole strip
  if (universe == 15)
  {
    FastLED.setBrightness(data[0]);
    FastLED.show();
  }

  // Store which universe has got in
  if ((universe - startUniverse) < maxUniverses) {
    universesReceived[universe - startUniverse] = 1;
  }

  for (int i = 0 ; i < maxUniverses ; i++)
  {
    if (universesReceived[i] == 0)
    {
      sendFrame = 0;
      break;
    }
  }

  // read universe and put into the right part of the display buffer
  for (int i = 0; i < length / 3; i++)
  {
    int led = i + (universe - startUniverse) * (previousDataLength / 3);
    if (led < numLeds)
      leds[led] = CRGB(data[i * 3], data[i * 3 + 1], data[i * 3 + 2]);
  }
  previousDataLength = length;

  //Display frame if all universes recieved
  if (sendFrame)
  {
    FastLED.show();
    // Reset universeReceived to 0
    memset(universesReceived, 0, maxUniverses);
  }
}



void setup() {
  
  // WiFi is started inside library
  SPIFFS.begin(); // Not really needed, checked inside library and started if needed
  ESPHTTPServer.begin(&SPIFFS);
  /* add setup code here */

  pinMode(ARTNET_INDICATION_LIGHT, OUTPUT);
  pinMode(LED_BUILTIN, OUTPUT);

  

  // Serial.begin(9600);
  artnet.begin();
  FastLED.addLeds<WS2812, ADDRESSABLE_LED_PIN, GRB>(leds, numLeds);
  initTest();

  // this will be called for each packet received
  artnet.setArtDmxCallback(onDmxFrame);
}

int readTemp(){
    if( millis() % 1000 == 0 ){
      float raw_voltage = analogRead(TEMPERATURE_SENSOR);
      float millivolts = raw_voltage / 1024 * (3300);
      float temperaturek = millivolts / 10;
      float temperaturec = temperaturek - 273.15;
      float temperaturef = (temperaturec * 1.8) + 32;

      // Serial.print("Temp K: ");
      // Serial.println(temperaturek);

      // Serial.print("Temp C: ");
      // Serial.println(temperaturec);


      // Serial.print("Temp F: ");
      // Serial.println(temperaturef);

      return temperaturef;
    }
  }


void loop() {

    artnet.read();

    if(lastFrameTime+50 < millis() || lastFrameTime > millis()){
      indicateDataRecieve(LOW);
    }

    int temperaturef = readTemp();

    if(millis() - lastDebug > 500){
      lastDebug = millis();
    //   //Put debug code here
    }

    // DO NOT REMOVE. Attend OTA update from Arduino IDE
    ESPHTTPServer.handle();	
}
