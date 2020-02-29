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
//

#include <ESP8266HTTPClient.h>
#include <ESP8266HTTPUpdateServer.h>
#include <ESP8266WebServer.h>
#include <ESP8266WiFi.h>
#include <ESP8266mDNS.h>
#include <WiFiClient.h>

#define wifi_ssid "ssid"
#define wifi_password "password"

int minute_blips = 0;
bool old_led_state = 0;

unsigned long oldMillis = 0;

WiFiClient espClient;

ESP8266WebServer httpServer(80);
ESP8266HTTPUpdateServer httpUpdater;

HTTPClient http;

// connessione WiFi
// ---------------------------------------------------------------------------|
void setup_wifi()
{
  delay(10);
  Serial.println("///////////////// - WiFi - /////////////////");
  Serial.print("Connessione a ");
  Serial.println(wifi_ssid);
  WiFi.mode(WIFI_STA);
  WiFi.hostname("contatore_palazzo");
  WiFi.begin(wifi_ssid, wifi_password);
  while (WiFi.status() != WL_CONNECTED)
  {
    delay(500);
    Serial.print(".");
  }
  Serial.println("Connesso");
  Serial.print("IP=");
  Serial.println(WiFi.localIP());
  Serial.println("////////////////////////////////////////////");
  Serial.println("");
}

// setup
// --------------------------------------------------------------------------------------|
void setup()
{
  pinMode(A0, INPUT);

  Serial.begin(115200);
  setup_wifi();

  httpUpdater.setup(&httpServer);
  httpServer.begin();
}

// loop
// ---------------------------------------------------------------------------------------|
void loop()
{
  oldMillis = millis();

  while ((millis() - oldMillis) < 60000)
  {
    httpServer.handleClient();
    if (analogRead(A0) > 550)
    {
      if (old_led_state == 0)
      {
        minute_blips = minute_blips + 1;
        old_led_state = 1;
        Serial.println("Blip");
      }
    }
    else
    {
      old_led_state = 0;
    }
    delay(50);
  }
  Serial.println("Minute");
  http.begin("http://192.168.1.2:8086/write?db=contatore");
  http.addHeader("Content-Type", "application/octet-stream");
  http.POST("blips,entity=contatore_palazzo value=" + String(minute_blips));
  minute_blips = 0;
  http.end();
}
