#if !defined(_OTA_UPDATE_)
#define _OTA_UPDATE_

#include <ArduinoOTA.h>

#include "hardware.h"

bool ota_uploading = false;
extern WiFiServer server;
extern const char* HOST;

void setupOTAUpdate() {
      Serial.print("\nInitializing OTA... ");

      ArduinoOTA.onStart([]()
                     {
                          detachInterrupt(PIN_ZERO_CROSS);
                          //timerEnd(timer_100ms);
                          server.stop();
                          ota_uploading = true;
                           String type;
                           if (ArduinoOTA.getCommand() == U_FLASH)
                           {
                               type = "sketch";
                           }
                           else
                           { // U_FS
                               type = "filesystem";
                           }
                           // NOTE: if updating FS this would be the place to unmount FS using FS.end()
                           Serial.println("Start updating " + type); });
  ArduinoOTA.onEnd([]()
                   { Serial.println("\nEnd"); });
  ArduinoOTA.onProgress([](unsigned int progress, unsigned int total)
                        {
                              digitalWrite(LED_PIN, HIGH);
                              Serial.printf("Progress: %u%%\r", (progress / (total / 100)));
                              digitalWrite(LED_PIN, LOW); });
  ArduinoOTA.onError([](ota_error_t error)
                     {
                           Serial.printf("Error[%u]: ", error);
                           if (error == OTA_AUTH_ERROR)
                           {
                               Serial.println("Auth Failed");
                           }
                           else if (error == OTA_BEGIN_ERROR)
                           {
                               Serial.println("Begin Failed");
                           }
                           else if (error == OTA_CONNECT_ERROR)
                           {
                               Serial.println("Connect Failed");
                           }
                           else if (error == OTA_RECEIVE_ERROR)
                           {
                               Serial.println("Receive Failed");
                           }
                           else if (error == OTA_END_ERROR)
                           {
                               Serial.println("End Failed");
                           } });
  ArduinoOTA.setHostname(HOST);
  ArduinoOTA.begin();
  Serial.println("OK");
  Serial.printf(".Hostname: %s.local\n", ArduinoOTA.getHostname());

}

#endif // _OTA_UPDATE_
