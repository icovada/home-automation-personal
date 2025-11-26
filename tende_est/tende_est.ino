/*
This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 3 of the License, or
(at your option) any later version.
This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
GNU General Public License for more details.
You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software Foundation,
Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301  USA
*/

// EEPROM data table
// 0 tNordPosition
// 1 tSudPosition
// 2 tNordDuration
// 3 tSudDuration
// 4 upSeconds
// 5 downSeconds

// sketch upload command
// curl -F "image=@tende_est.ino.bin" http://192.168.1.18/update -v

#define MQTT_NAME "tende_est"
#define MQTT_HUMAN_NAME "Tende Est"

#include <math.h>
#include <EEPROM.h>
#include <ESP8266HTTPUpdateServer.h>
#include <ESP8266WebServer.h>
#include <ESP8266WiFi.h>
#include <WiFiClient.h>
#include <ArduinoHA.h>
#include "credentials.h"

WiFiClient wifiMqttClient;
HADevice device(MQTT_NAME);
HAMqtt mqtt(wifiMqttClient, device);
char mqtt_server[32] = "";

ESP8266WebServer httpServer(80);
ESP8266HTTPUpdateServer httpUpdater;

class TendaManager
{
public:
  int pinUp = -1;
  int pinDown = -1;
  int timeUp = -1;
  int timeDown = -1;
  HACover *tenda;
  unsigned long startMillis = 0;
  unsigned long stopMillis = 0;
  unsigned long lastUpdate = 0;

  TendaManager(int pinUp, int pinDown, int timeUp, int timeDown, HACover *tenda)
      : pinUp(pinUp),
        pinDown(pinDown),
        timeUp(timeUp),
        timeDown(timeDown),
        tenda(tenda)
  {
    pinMode(pinUp, OUTPUT);
    pinMode(pinDown, OUTPUT);
    digitalWrite(pinUp, 0);
    digitalWrite(pinDown, 0);
  }

  void close()
  {
    // go up
    stop(false);
    tenda->setCurrentState(HACover::CoverState::StateOpening);
    startMillis = millis();
    digitalWrite(pinUp, true);
  }

  void open()
  {
    // go down
    stop(false);
    tenda->setCurrentState(HACover::CoverState::StateClosing);
    startMillis = millis();
    digitalWrite(pinDown, true);
  }

  void stop(bool send = true)
  {
    if (send)
    {
      if (tenda->getCurrentState() == HACover::CoverState::StateClosing)
      {
        tenda->setCurrentState(HACover::CoverState::StateClosed);
      }
      else if (tenda->getCurrentState() == HACover::CoverState::StateOpening)
      {
        tenda->setCurrentState(HACover::CoverState::StateOpen);
      }
      else
      {
        tenda->setCurrentState(HACover::CoverState::StateStopped);
      }
    }
    digitalWrite(pinDown, false);
    digitalWrite(pinUp, false);
    lastUpdate = millis();
    stopMillis = -1;
    sendPosition();
  }

  void check()
  {
    if (tenda->getCurrentState() == HACover::CoverState::StateOpening || tenda->getCurrentState() == HACover::CoverState::StateClosing)
    {
      if (millis() >= stopMillis)
      {
        stop();
      } else if (millis() == -1){
        if ((millis() - 35000) > startMillis ){
          // it had enough
          stop();
          return;
        }
      }
      sendPosition();
    }
    
  }

  void setPosition(int position)
  {
    int percentage_to_go = position - tenda->getCurrentPosition();
    if (percentage_to_go > 0)
    {
      // open, go down
      open();
      stopMillis = millis() + (timeDown / 100 * abs(percentage_to_go));
    }
    else if (percentage_to_go < 0)
    {
      // close, go up
      close();
      stopMillis = millis() + (timeUp / 100 * abs(percentage_to_go));
    }
  }

  void sendPosition()
  {
    if (millis() - 1000 > lastUpdate)
    {
      int curPosition = tenda->getCurrentPosition();
      int delta = 0;
      if (tenda->getCurrentState() == HACover::CoverState::StateOpening)
      {
        delta = (curPosition - (millis() - startMillis) / ((timeUp * 1000) / 100));
        tenda->setPosition(tenda->getCurrentPosition() - delta);
      }
      else
      {
        delta = (curPosition - (millis() - startMillis) / ((timeDown * 1000) / 100));
        tenda->setPosition(tenda->getCurrentPosition() + delta);
      }
      lastUpdate = millis();
    }
  }
};

HACover ha_nord("tenda_est_nord", HACover::Features::PositionFeature);
HACover ha_sud("tenda_est_sud", HACover::Features::PositionFeature);

TendaManager nord(D1, D2, 35, 30, &ha_nord);
TendaManager sud(D6, D7, 35, 30, &ha_sud);

// setup
// --------------------------------------------------------------------------------------|
void setup()
{
  Serial.begin(115200);

  ha_nord.setDeviceClass("awning");
  ha_nord.setName("Tenda Est Nord");

  ha_sud.setDeviceClass("awning");
  ha_sud.setName("Tenda Est Sud");

  WiFi.mode(WIFI_STA);
  WiFi.begin(wifi_ssid, wifi_password);
  while (WiFi.status() != WL_CONNECTED)
  {
    delay(500);
    Serial.print(".");
  }

  device.setName(MQTT_HUMAN_NAME);
  device.setUniqueId((const byte *)MQTT_NAME, 9);

  device.enableSharedAvailability();
  device.enableLastWill();

  mqtt.begin("mqtt.in.tabbo.it");

  httpUpdater.setup(&httpServer);
  httpServer.begin();
}

void loop()
{
  // Reconnect to MQTT if connection lost
  if (!mqtt.isConnected() && mqtt_server[0] != '\0')
  {
    mqtt.begin(mqtt_server);
  }

  mqtt.loop();
  httpServer.handleClient();

  nord.check();
  sud.check();

}
