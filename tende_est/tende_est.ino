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
  unsigned long stopMillis = -1;
  unsigned long lastUpdate = 0;
  int startPosition = 0;
  int eepromAddress = -1;

  TendaManager(int pinUp, int pinDown, int timeUp, int timeDown, HACover *tenda, int eepromAddress)
      : pinUp(pinUp),
        pinDown(pinDown),
        timeUp(timeUp),
        timeDown(timeDown),
        tenda(tenda),
        eepromAddress(eepromAddress)
  {
    pinMode(pinUp, OUTPUT);
    pinMode(pinDown, OUTPUT);
    digitalWrite(pinUp, 0);
    digitalWrite(pinDown, 0);
  }

  void loadPosition()
  {
    if (eepromAddress >= 0)
    {
      int savedPosition = EEPROM.read(eepromAddress);
      if (savedPosition <= 100)
      {
        tenda->setPosition(savedPosition);
        Serial.print("Loaded position: ");
        Serial.println(savedPosition);
      }
      else
      {
        // Invalid EEPROM value, set to 0
        tenda->setPosition(100);
        Serial.println("No valid saved position, setting to 0");
      }

      if (savedPosition != 100){
        open();
      }
    }
  }

  void savePosition()
  {
    if (eepromAddress >= 0)
    {
      int currentPosition = tenda->getCurrentPosition();
      EEPROM.write(eepromAddress, currentPosition);
      EEPROM.commit();
      Serial.print("Saved position: ");
      Serial.println(currentPosition);
    }
  }

  void close()
  {
    // go up
    Serial.println("Close");
    stop(true, false);
    tenda->setState(HACover::CoverState::StateClosing);
    startMillis = millis();
    startPosition = tenda->getCurrentPosition();
    digitalWrite(pinUp, true);
  }

  void open()
  {
    // go down
    Serial.println("Open");
    stop(true, false);
    tenda->setState(HACover::CoverState::StateOpening);
    startMillis = millis();
    startPosition = tenda->getCurrentPosition();
    // move one step open before saving so it's always marked open
    tenda->setCurrentPosition(startPosition-1);
    savePosition();
    digitalWrite(pinDown, true);
  }

  void stop(bool simple = false, bool send = true)
  {
    Serial.println("void stop");
    int curPosition = getPosition();
    if (curPosition == 100)
    {
      tenda->setState(HACover::CoverState::StateOpen);
    }
    if (send)
    {
      if (tenda->getCurrentState() == HACover::CoverState::StateClosing)
      {
        tenda->setState(HACover::CoverState::StateClosed);
      }
      else if (tenda->getCurrentState() == HACover::CoverState::StateOpening)
      {
        if (getPosition() == 100)
        {
          tenda->setState(HACover::CoverState::StateOpen);
        }
        else
        {
          tenda->setState(HACover::CoverState::StateStopped);
        }
      }
      lastUpdate = millis();
    }
    tenda->setPosition(curPosition);
    digitalWrite(pinDown, false);
    digitalWrite(pinUp, false);

    if (!simple)
    {
      Serial.println("void stop reset");
      stopMillis = -1;
      sendPosition();
      savePosition();
    }
  }

  void check()
  {
    if (tenda->getCurrentState() == HACover::CoverState::StateOpening || tenda->getCurrentState() == HACover::CoverState::StateClosing)
    {
      if (stopMillis == -1)
      {
        if (millis() - startMillis > 40000)
        {
          // it had enough
          Serial.println("Stop end run");
          stop();
          return;
        }
      }
      else if (millis() >= stopMillis)
      {
        Serial.println("Stop reached target");
        stop();
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

  int getPosition()
  {
    if (tenda->getCurrentState() == HACover::CoverState::StateOpening)
    {
      // Opening = moving up (retracting) = increasing position toward 100
      int elapsed = (millis() - startMillis) * 100 / (timeUp * 1000);
      return min(startPosition + elapsed, 100);
    }
    else if (tenda->getCurrentState() == HACover::CoverState::StateClosing)
    {
      // Closing = moving down (extending) = decreasing position toward 0
      int elapsed = (millis() - startMillis) * 100 / (timeDown * 1000);
      return max(startPosition - elapsed, 0);
    }
    else
    {
      return tenda->getCurrentPosition();
    }
  }

  void sendPosition()
  {
    int elapsed = -1;
    if (millis() - 1000 > lastUpdate)
    {
      int position = getPosition();
      tenda->setPosition(position);
      lastUpdate = millis();
    }
  }
};

HACover ha_nord("tenda_est_nord", HACover::Features::PositionFeature);
HACover ha_sud("tenda_est_sud", HACover::Features::PositionFeature);

TendaManager nord(D1, D2, 35, 30, &ha_nord, 0);
TendaManager sud(D6, D7, 35, 30, &ha_sud, 1);

void onCommand(HACover::CoverCommand cmd, HACover *sender)
{
  if (cmd == HACover::CoverCommand::CommandClose)
  {
    Serial.println("Received Close");
    if (nord.tenda == sender)
    {
      nord.close();
    }
    else if (sud.tenda == sender)
    {
      sud.close();
    }
  }
  else if (cmd == HACover::CoverCommand::CommandOpen)
  {
    Serial.println("Received Open");
    if (nord.tenda == sender)
    {
      nord.open();
    }
    else if (sud.tenda == sender)
    {
      sud.open();
    }
  }
  else if (cmd == HACover::CoverCommand::CommandStop)
  {
    Serial.println("Received Stop");
    if (nord.tenda == sender)
    {
      nord.stop();
    }
    else if (sud.tenda == sender)
    {
      sud.stop();
    }
  }
}

// setup
// --------------------------------------------------------------------------------------|
void setup()
{
  Serial.begin(115200);

  // Initialize EEPROM
  EEPROM.begin(512);

  ha_nord.setDeviceClass("blind");
  ha_nord.setName("Tenda Est Nord");
  ha_nord.onCommand(onCommand);

  ha_sud.setDeviceClass("blind");
  ha_sud.setName("Tenda Est Sud");
  ha_sud.onCommand(onCommand);

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

  // Wait for MQTT connection and load/set initial positions
  while (!mqtt.isConnected())
  {
    mqtt.loop();
    delay(100);
  }

  // Load positions from EEPROM or set to 0 if not valid
  nord.loadPosition();
  sud.loadPosition();

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
