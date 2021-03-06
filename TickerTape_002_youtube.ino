/* ***************************************************************************************************** *\
**     TickerTape Firmware                                                                               **
**     (c) 2018 Darren Dignam                                                                            **
**                                                                                                       **
**                                                                                                       **
**     This project relies on the community of ESP32 developers, and AdaFruit and others.                **
**     This version shows how to parse the youtube API                                                   **
**                                                                                                       **
**     Special shout out to this project:                                                                **
**     https://github.com/zhouhan0126/WIFIMANAGER-ESP32                                                  **
**                                                                                                       **
\* ***************************************************************************************************** */

//Defines and constants  (Version might be better as date stting, and used for headers..?)
#define VERSION "TickerTape v0.2"
#define DEBUG_BUFFER_SIZE 500
int DEBUG = 1;

//The state machine for the device. 0 is the default, display the time and show the sub count too. 1 is time only. 2 is subcount only!
int DeviceMode = 0;
int Subscribers = 0;  //can use this to track the last subscriber value

unsigned long p_time_Millis = 0;        // will store last time LED was updated
unsigned long p_subs_Millis = 0;
unsigned long p_delay_Millis = 0;
const long    millis_interval = 1000;

uint16_t UI_Leds = 0b0000000000000000;  //all the additional UI LEDS are off.

#include "LED_Display_Wrapper.h"
LED_Display_Wrapper LEDdisplay = LED_Display_Wrapper();

//ESP On board LED
#define LED 5

//Wifi Stack and storage
#include "SPIFFS.h"
#include <WiFi.h>
#include <DNSServer.h>
#include <ESP8266WebServer.h>
#include <WiFiManager.h>          //https://github.com/tzapu/WiFiManager
#include <ArduinoJson.h>          //https://github.com/bblanchon/ArduinoJson
#include <HTTPClient.h>

//Youtube API anyone?
#include <YoutubeApi.h>

char apiKey[45] = "YOUR_YOUTUBE_API_KEY_HERE";   //our API, should be a secret!
char channelId[30] = "UCQak2_fXZ_9yXI5vB_Kd54g";  //diodegonewild

unsigned long api_mtbs = 60000; //mean time between api requests
unsigned long api_lasttime;   //last time api request has been done
unsigned long subs = 0;
WiFiClientSecure client;
YoutubeApi *api;

//Time params
#include <time.h>

short timezone = 1;

char displaybuffer[6] = {' ',' ',' ',' ',' ',' '};
char _str_buffer[7];  //6 chars and a null char...

//define default values here, if there are different values in config.json, they are overwritten.
char Brightness[40];
char WelcomeText[50];

uint8_t _brightness = 8;
uint8_t _brightness_new;

//touch buttons stuff
int touch_threshold = 40;  //Adjust this if touch is not working.
void gotTouch0(){  
  //brightness down
  _brightness_new = _brightness - 1;

  if(_brightness_new <= 1){
    _brightness_new = 1;
  }
  //LEDdisplay.setBrightness(_brightness);
  //delay(10);
}
void gotTouch3(){
  //brightness up
    _brightness_new = _brightness + 1;
  if(_brightness_new >= 14){
    _brightness_new = 14;
  }
  //LEDdisplay.setBrightness(_brightness);
}
void gotTouch5(){
  DeviceMode = 1;
}
void gotTouch4(){
  DeviceMode = 2;
}

#include "helper_functions.h"

//flag for saving data
bool shouldSaveConfig = true;

//callback notifying us of the need to save config
void saveConfigCallback () {
  Serial.println("Should save config");
  shouldSaveConfig = true;
}

void setup() {
  //ResetDevice();

  
  //init strings to empty
  Brightness[0] = 0;
  WelcomeText[0] = 0;

  //ESP LED
  pinMode(LED, OUTPUT);
  digitalWrite(LED, LOW);

  Serial.begin(115200);
  Serial.println();

  //bind touch inturrupts
  touchAttachInterrupt(T0, gotTouch0, touch_threshold);
  touchAttachInterrupt(T3, gotTouch3, touch_threshold);
  touchAttachInterrupt(T4, gotTouch4, touch_threshold);
  touchAttachInterrupt(T5, gotTouch5, touch_threshold);

  //debug..
  dbgprint ( "Starting ESP32-TickerTape running on CPU %d at %d MHz.  Version %s.  Free memory %d", xPortGetCoreID(), ESP.getCpuFreqMHz(), VERSION, ESP.getFreeHeap() );

  LEDdisplay.BLINK();LEDdisplay.BLINK();LEDdisplay.BLINK();  //LEDdisplay.BLINK();LEDdisplay.BLINK();

  //read configuration from FS json
  dbgprint("mounting FS...");
  SPIFFS.begin (true);

  if (SPIFFS.begin()) {
    dbgprint("mounted file system");
    if (SPIFFS.exists("/config.json")) {
      //file exists, reading and loading
      dbgprint("reading config file");
      File configFile = SPIFFS.open("/config.json", "r");
      if (configFile) {
        dbgprint("opened config file");
        size_t size = configFile.size();
        // Allocate a buffer to store contents of the file.
        std::unique_ptr<char[]> buf(new char[size]);

        configFile.readBytes(buf.get(), size);
        DynamicJsonBuffer jsonBuffer;
        JsonObject& json = jsonBuffer.parseObject(buf.get());
        json.printTo(Serial);
        if (json.success()) {
          Serial.println("\nparsed json");

          strcpy(Brightness, json["Brightness"]);
          strcpy(WelcomeText, json["WelcomeText"]);
                                              // strcpy(Duty_Cycle, json["Duty_Cycle"]);

        } else {
          dbgprint("failed to load json config");
        }
      }
    }
  } else {
    dbgprint("failed to mount FS");
  }
  //end read

  //do something with settings..??
  if( WelcomeText == "RESET" ){
    dbgprint("ERASING MEMORY...");
    LEDdisplay.ScrollText("Resetting device...     ");
    ResetDevice();
    LEDdisplay.ScrollText("Device reset. Turn me off and back on again :)       ");
  }
  if( Brightness != "" ){
    dbgprint("Setting brightness...");
    dbgprint(Brightness);
    String _tmp_bright = Brightness;
    if(_tmp_bright.toInt() > 0){
      _brightness = _tmp_bright.toInt();
      _brightness_new = _brightness;
      LEDdisplay.setBrightness( _tmp_bright.toInt() );
      Serial.print("Saved Brightness: ");
      Serial.println(_brightness);
    }
  }

  String tmpString = "";
  
  // if( WelcomeText == "" ){
  if(strlen(WelcomeText) == 0){
    tmpString = "TICKERTAPE - DIODEGONEWILD";
  }else{
    tmpString = WelcomeText;
  }
  String _message = "v0.2 " + tmpString + "      ";
  LEDdisplay.ScrollText( _message );




                                        // // The extra parameters to be configured (can be either global or just in the setup)
                                        // // After connecting, parameter.getValue() will get you the configured value
                                        // // id/name placeholder/prompt default length
  WiFiManagerParameter custom_Brightness("Brightness", "Brightness", Brightness, 40);
  WiFiManagerParameter custom_WelcomeText("WelcomeText", "WelcomeText", WelcomeText, 6);

  //WiFiManager
  //Local intialization. Once its business is done, there is no need to keep it around
  WiFiManager wifiManager;

  //set config save notify callback
  wifiManager.setSaveConfigCallback(saveConfigCallback);

  //The custom parameters here
  wifiManager.addParameter(&custom_Brightness);
  wifiManager.addParameter(&custom_WelcomeText);

  //fetches ssid and pass and tries to connect
  //if it does not connect it starts an access point with the specified name
  //here  "AutoConnectAP"
  //and goes into a blocking loop awaiting configuration
  if (!wifiManager.autoConnect("TickerTape", "G42")) {
    Serial.println("failed to connect and hit timeout");
    delay(3000);
    //reset and try again, or maybe put it to deep sleep
    ESP.restart();
    delay(5000);
  }

  //if you get here you have connected to the WiFi
  Serial.println("connected... great :)");

  //read custom parameters
  strcpy(Brightness, custom_Brightness.getValue());
  strcpy(WelcomeText, custom_WelcomeText.getValue());

  //save the custom parameters to FS
  if (shouldSaveConfig) {
    Serial.println("saving config");
    DynamicJsonBuffer jsonBuffer;
    JsonObject& json = jsonBuffer.createObject();
    
    json["Brightness"] = Brightness;
    json["WelcomeText"] = WelcomeText;

    File configFile = SPIFFS.open("/config.json", "w");
    if (!configFile) {
      Serial.println("failed to open config file for writing");
    }
    json.printTo(Serial);
    json.printTo(configFile);
    configFile.close();
    //end save
  }

  Serial.println("local ip");
  Serial.println(WiFi.localIP());

  String _tmp_IP = WiFi.localIP().toString().c_str();
  LEDdisplay.ScrollText("IP "+ _tmp_IP +"      ");
  
  Serial.println();
  Serial.print("Brightness: ");
  Serial.print(Brightness);
  Serial.println();
  Serial.print("WelcomeText: ");
  Serial.print(WelcomeText);

  //WiFi.disconnect(true); //erases store credentially
  //SPIFFS.format();  //erases stored values
  Serial.println("Done");

  //setup youtube API stuff
  api = new YoutubeApi(apiKey, client);

  //setup time stuff
  configTime(timezone * 3600, 0, "pool.ntp.org", "time.nist.gov");
  Serial.println("\nWaiting for time");
  while (!time(nullptr)) {
    Serial.print(".");
    delay(1000);
  }

  //TODO: Fix the line below, a bit of a hack. The WiFi Manager resets the brightness, as it uses it own LED class to write text at 50% brightness, and never restets it to the saved value
  LEDdisplay.setBrightness(_brightness);

  //turn on the wifi LED
  UI_Leds = 0b0000100000000000;
  LEDdisplay.writeDigitRaw(6, UI_Leds);
  LEDdisplay.writeDisplay();
}

void loop() {
  unsigned long currentMillis = millis();
  if(_brightness_new != _brightness){
    _brightness = _brightness_new;
    LEDdisplay.setBrightness(_brightness);
    SaveConfig();
  }
  if(currentMillis > p_delay_Millis){
    if(DeviceMode == 0 || DeviceMode == 1){  //time stuff
      //p_time_Millis
      if (currentMillis - p_time_Millis >= (millis_interval/3) ) {
        UI_Leds &= ~(0b0000000011000000);  //toggle the seconds seperator bits
        LEDdisplay.writeDigitRaw(6, UI_Leds); 
        LEDdisplay.writeDisplay();
  //      p_time_Millis = currentMillis;  // this is only the 1/2 way point, reset this below
      }
      if (currentMillis - p_time_Millis >= millis_interval ) {
        time_t now = time(nullptr); //get time 
        struct tm * timeinfo;
        timeinfo = localtime(&now);  //parse time
        //display time
        sprintf(_str_buffer, "%02d%02d%02d", timeinfo->tm_hour, timeinfo->tm_min, timeinfo->tm_sec);
        LEDdisplay.FillTextBuffer(_str_buffer);

        UI_Leds |= 0b0000000011001100;  //Turn on the time seperator bits;
        LEDdisplay.writeDigitRaw(6, UI_Leds); 
        LEDdisplay.writeDisplay();

        //every min
        if(DeviceMode == 0 && timeinfo->tm_sec == 0 ){
          //probably use an inturrupt to do this, and only update when new subscribers arrive..?
          parseSubs(true);//Get the youtube subscribers...
        }

        p_time_Millis = currentMillis;
      }    
    }else if(DeviceMode == 2){
      if (currentMillis - p_subs_Millis >= (60 * millis_interval) ) {
        p_subs_Millis = currentMillis;
        parseSubs(false);//Get the youtube subscribers...
      }
    }
  }


}    // **  END LOOP  **




// ** ****************************************************************************************************  **
// **                                                                                                       **
// **  This function is useful to erase the saved settings, including any known WiFi Access Points          **
// **                                                                                                       **
// ** ****************************************************************************************************  **
void ResetDevice(){
  WiFi.disconnect(true); //erases store credentially
  SPIFFS.format();  //erases stored values
  ESP.restart();
}

