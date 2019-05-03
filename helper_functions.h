////**************************************************************************************************
////                                          D B G P R I N T                                        *
////**************************************************************************************************
//// Send a line of info to serial output.  Works like vsprintf(), but checks the DEBUG flag.        *
//// Print only if DEBUG flag is true.  Always returns the formatted string.                         *
////**************************************************************************************************
char* dbgprint ( const char* format, ... )
{
  static char sbuf[DEBUG_BUFFER_SIZE] ;                // For debug lines
  va_list varArgs ;                                    // For variable number of params

  va_start ( varArgs, format ) ;                       // Prepare parameters
  vsnprintf ( sbuf, sizeof(sbuf), format, varArgs ) ;  // Format the message
  va_end ( varArgs ) ;                                 // End of using parameters
  if ( DEBUG )                                         // DEBUG on?
  {
    Serial.print ( "D: " ) ;                           // Yes, print prefix
    Serial.println ( sbuf ) ;                          // and the info
  }
  return sbuf ;                                        // Return stored string
}

void parseSubs(bool scroll_message){
    //clear display!
//    LEDdisplay.writeDigitRaw(6, 0b0000000000000000); 
    UI_Leds &= ~(0b0000000011001100);  //toggle the time seperator bits
    LEDdisplay.writeDigitRaw(6, UI_Leds); 

    //get data!
    if(api->getChannelStatistics(channelId))
    {
      Serial.println("---------Stats---------");
      Serial.print("Subscriber Count: ");
      Serial.println(api->channelStats.subscriberCount);
      Serial.print("View Count: ");
      Serial.println(api->channelStats.viewCount);
      Serial.print("Comment Count: ");
      Serial.println(api->channelStats.commentCount);
      Serial.print("Video Count: ");
      Serial.println(api->channelStats.videoCount);
      // Probably not needed :)
      //Serial.print("hiddenSubscriberCount: ");
      //Serial.println(api->channelStats.hiddenSubscriberCount);
      Serial.println("------------------------");

//      String _tmp;// = api->channelStats.subscriberCount;

      if(api->channelStats.subscriberCount > subs){
        LEDdisplay.BLINK();
        UI_Leds = 0b0000100000000000;
        LEDdisplay.writeDigitRaw(6, UI_Leds);
        LEDdisplay.writeDisplay();
      }
      subs = api->channelStats.subscriberCount;

      sprintf(_str_buffer, "%06d", api->channelStats.subscriberCount);
      if(scroll_message){
        LEDdisplay.FillTextBuffer("      ");
        LEDdisplay.ScrollText("SUBSCRIBER COUNT  ");
        LEDdisplay.ScrollText(_str_buffer);
//      delay(10000);
      }else{
        LEDdisplay.FillTextBuffer(_str_buffer);
      }
      p_delay_Millis = millis()+10000;
    }
}

void SaveConfig(){
//    Serial.println("saving config");
    DynamicJsonBuffer jsonBuffer;
    JsonObject& json = jsonBuffer.createObject();
    String _bright = String(_brightness);
    _bright.toCharArray(Brightness, 40);
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

