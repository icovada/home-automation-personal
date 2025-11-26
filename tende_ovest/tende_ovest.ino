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

#define MQTT_NAME "tende_ovest"
#define MQTT_HUMAN_NAME "Tende Ovest"

#include <math.h>
#include <EEPROM.h>
#include <ESP8266HTTPUpdateServer.h>
#include <ESP8266WebServer.h>
#include <ESP8266WiFi.h>
#include <WiFiClient.h>
#include <ArduinoHA.h>
#include "credentials.h"
#include "tende.h"

WiFiClient wifiMqttClient;
HADevice device(MQTT_NAME);
HAMqtt mqtt(wifiMqttClient, device);
char mqtt_server[32] = "";

ESP8266WebServer httpServer(80);
ESP8266HTTPUpdateServer httpUpdater;

HACover ha_nord("tenda_ovst_nord", HACover::Features::PositionFeature);
HACover ha_sud("tenda_ovest_sud", HACover::Features::PositionFeature);
HAButton telecomando("telecomando_cancello");

TendaManager nord(D1, D2, 35, 30, &ha_nord, 0);
TendaManager sud(D6, D7, 35, 30, &ha_sud, 1);

void telecomandoOnCommand(HAButton *sender){
  digitalWrite(D8, LOW);
  delay(1000);
  digitalWrite(D8, HIGH);
}

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

  // set telecomando
  pinMode(D8, OUTPUT);
  digitalWrite(D8, 1);

  // Initialize EEPROM
  EEPROM.begin(512);

  ha_nord.setDeviceClass("blind");
  ha_nord.setName("Tenda Est Nord");
  ha_nord.onCommand(onCommand);

  ha_sud.setDeviceClass("blind");
  ha_sud.setName("Tenda Est Sud");
  ha_sud.onCommand(onCommand);

  telecomando.setName("Telecomando Cancello");
  telecomando.setIcon("mdi:gate-open");
  telecomando.onCommand(telecomandoOnCommand);

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
