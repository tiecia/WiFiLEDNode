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

#include <ArtnetnodeWifi.h>
#include <Arduino.h>
#include <Adafruit_NeoPixel.h>
#include <string>
#include <cstring>

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
int numLeds = NUM_LEDS; // CHANGE FOR YOUR SETUP
int addressableledpin = ADDRESSABLE_LED_PIN;
int numberOfChannels; // Total number of channels you want to receive (1 led = 3 channels)
Adafruit_NeoPixel leds = Adafruit_NeoPixel();

//Status LED settings, denote whether their respective led will be enabled or disabled.
int internalled = INTERNAL_LED;
int externalled = EXTERNAL_LED;


// Art-Net settings
ArtnetnodeWifi artnet;
int startUniverse = ARTNET_START_UNIVERSE;
String artnetbroadcastname = BROADCAST_NAME;


int maxUniverses;
bool *universesReceived;
bool sendFrame = 1;
int previousDataLength = 0;
int lastFrameTime;

int lastDebug;

void indicateDataRecieve(bool state){
  if(externalled){
    digitalWrite(ARTNET_INDICATION_LIGHT, state);
  }
  if(internalled){
    digitalWrite(LED_BUILTIN, !state);
  }
}

void initTest()
{
  for (int i = 0 ; i < numLeds ; i++) {
    leds.setPixelColor(i, 127, 0, 0);
  }
  leds.show();
  delay(500);
  for (int i = 0 ; i < numLeds ; i++) {
    leds.setPixelColor(i, 0, 127, 0);
  }
  leds.show();
  delay(500);
  for (int i = 0 ; i < numLeds ; i++) {
    leds.setPixelColor(i, 0, 0, 127);
  }
  leds.show();
  delay(500);
  for (int i = 0 ; i < numLeds ; i++) {
    leds.setPixelColor(i, 0, 0, 0);
  }
  leds.show();
}

void onDmxFrame(uint16_t universe, uint16_t length, uint8_t sequence, uint8_t* data)
{
  lastFrameTime = millis();
  indicateDataRecieve(HIGH);


  sendFrame = 1;

  // set brightness of the whole strip
  if (universe == 15)
  {
    leds.setBrightness(data[0]);
    leds.show();
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
    leds.setPixelColor(led, data[i * 3], data[i * 3 + 1], data[i * 3 + 2]);
  }
  previousDataLength = length;

  //Display frame if all universes recieved
  if (sendFrame)
  {
    leds.show();
    // Reset universeReceived to 0
    memset(universesReceived, 0, maxUniverses);
  }
}



void setup() {
  
  // WiFi is started inside library
  SPIFFS.begin(); // Not really needed, checked inside library and started if needed
  ESPHTTPServer.begin(&SPIFFS);
  /* add setup code here */

  //Fetch user settings from FSServer if config file exists. If file does not exist create one and write the default values to it.
  String broadcast_fetch;
  ESPHTTPServer.load_user_config("broadcastname", broadcast_fetch);
  if(broadcast_fetch == ""){
    ESPHTTPServer.save_user_config("lednum", numLeds);
    ESPHTTPServer.save_user_config("ledpin", addressableledpin);
    ESPHTTPServer.save_user_config("broadcastname", artnetbroadcastname);
    ESPHTTPServer.save_user_config("startuniverse", startUniverse);
    ESPHTTPServer.save_user_config("enablebuiltinled", internalled);
    ESPHTTPServer.save_user_config("enableexternalled", externalled);
  } else {
    artnetbroadcastname = broadcast_fetch;
    ESPHTTPServer.load_user_config("lednum", numLeds);
    ESPHTTPServer.load_user_config("ledpin", addressableledpin);
    ESPHTTPServer.load_user_config("startuniverse", startUniverse);
    ESPHTTPServer.load_user_config("enablebuiltinled", internalled);
    ESPHTTPServer.load_user_config("enableexternalled", externalled);
  }



  //Apply user FSServer user settings
  leds.updateType(NEO_GRB + NEO_KHZ800);
  leds.updateLength(numLeds);
  leds.setPin(addressableledpin);
  
  char name[artnetbroadcastname.length() + 1];
  strcpy(name, artnetbroadcastname.c_str());
  artnet.setName(name);

  numberOfChannels = numLeds * 3;
  maxUniverses = numberOfChannels / 512 + ((numberOfChannels % 512) ? 1 : 0);
  universesReceived = (bool *)malloc(sizeof(maxUniverses));

  //Set status led pin modes
  pinMode(ARTNET_INDICATION_LIGHT, OUTPUT);
  pinMode(LED_BUILTIN, OUTPUT);

  leds.begin();
  artnet.begin();
  artnet.setArtDmxCallback(onDmxFrame);

  initTest();

  //Note: HIGH on the Wemos D1 builtin led is off.
  digitalWrite(LED_BUILTIN, HIGH);
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
      //Put debug code here
    }

    // DO NOT REMOVE. Attend OTA update from Arduino IDE
    ESPHTTPServer.handle();	
}
