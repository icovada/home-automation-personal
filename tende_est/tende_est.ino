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

#include <EEPROM.h>
#include <ESP8266HTTPUpdateServer.h>
#include <ESP8266WebServer.h>
#include <ESP8266WiFi.h>
#include <PubSubClient.h>
#include <WiFiClient.h>

#define wifi_ssid "ssid"
#define wifi_password "password"

#define mqtt_server "192.168.1.2"
#define mqtt_id "tendeest"

#define topic_tende_set_nord "roncello/esterno/est/set/nord"
#define topic_tende_set_sud "roncello/esterno/est/set/sud"
#define topic_tende_set_group "roncello/esterno/est/set/group"

#define topic_tende_status_nord "roncello/esterno/est/status/nord"
#define topic_tende_status_sud "roncello/esterno/est/status/sud"

#define topic_upseconds_set "roncello/esterno/est/set/upseconds"
#define topic_downseconds_set "roncello/esterno/est/set/downseconds"

unsigned long tNordActBegin = 0;
unsigned long tSudActBegin = 0;
unsigned long tNordDuration = 0;
unsigned long tSudDuration = 0;
unsigned long lastMQTT = 0;
bool tNordGoingUp = false;
bool tSudGoingUp = false;
bool tNordMoving = false;
bool tSudMoving = false;

unsigned long oldMillisNord = 0;
unsigned long oldMillisSud = 0;

WiFiClient espClient;
PubSubClient client(espClient);

String sTopic;
String sPayload;

int tNordPosition;
int tSudPosition;

int upSeconds;
int downSeconds;

ESP8266WebServer httpServer(80);
ESP8266HTTPUpdateServer httpUpdater;

int setNord(int position) {
  Serial.println("SetNord");
  tNordActBegin = millis();
  tNordMoving = true;
  oldMillisNord = 0;
  if (position == 100) {
    digitalWrite(D1, LOW);
    digitalWrite(D2, HIGH);
    tNordGoingUp = false;
    tNordDuration = downSeconds * 1000 + 15000;
  } else if (position == 0) {
    digitalWrite(D1, HIGH);
    digitalWrite(D2, LOW);
    tNordGoingUp = true;
    tNordDuration = upSeconds * 1000 + 15000;
  } else if (position > tNordPosition) {
    digitalWrite(D1, LOW);
    digitalWrite(D2, HIGH);
    tNordGoingUp = false;
    // Map position in 100 degrees to total seconds it takes
    tNordDuration = downSeconds * 10 * (position - tNordPosition);
  } else if (position < tNordPosition) {
    digitalWrite(D1, HIGH);
    digitalWrite(D2, LOW);
    tNordGoingUp = true;
    tNordDuration = upSeconds * 10 * (tNordPosition - position);
  }
  Serial.print("Target: ");
  Serial.println(tNordDuration);
  EEPROM.write(2, position);
  EEPROM.commit();
}

int setSud(int position) {
  Serial.println("SetSud");
  tSudActBegin = millis();
  tSudMoving = true;
  oldMillisSud = 0;
  if (position == 100) {
    digitalWrite(D6, LOW);
    digitalWrite(D7, HIGH);
    tSudGoingUp = false;
    tSudDuration = downSeconds * 1000 + 15000;
  } else if (position == 0) {
    digitalWrite(D6, HIGH);
    digitalWrite(D7, LOW);
    tSudGoingUp = true;
    tSudDuration = upSeconds * 1000 + 15000;
  } else if (position > tSudPosition) {
    digitalWrite(D6, LOW);
    digitalWrite(D7, HIGH);
    tSudGoingUp = false;
    // Map position in 100 degrees to total seconds it takes
    tSudDuration = downSeconds * 10 * (position - tSudPosition);
  } else if (position < tSudPosition) {
    digitalWrite(D6, HIGH);
    digitalWrite(D7, LOW);
    tSudGoingUp = true;
    tSudDuration = upSeconds * 10 * (tSudPosition - position);
  }
  Serial.print("Target: ");
  Serial.println(tSudDuration);
  EEPROM.write(3, position);
  EEPROM.commit();
}

int whereInTheWorldIsTenda(unsigned long actBegin, bool goingUp, int tPosition,
                           char *topic) {
  unsigned long tTime = millis() - actBegin;
  int newPosition;
  if (goingUp == true) {
    newPosition = posValidator(tPosition - tTime / ((upSeconds * 1000) / 100));
  } else {
    newPosition =
        posValidator(tPosition + tTime / ((downSeconds * 1000) / 100));
  }
  client.publish(topic, String(newPosition).c_str());
  return newPosition;
}

int stopNord() {
  digitalWrite(D1, LOW);
  digitalWrite(D2, LOW);
  tNordMoving = false;
  int newPosition = whereInTheWorldIsTenda(
      tNordActBegin, tNordGoingUp, tNordPosition, topic_tende_status_nord);
  EEPROM.write(0, newPosition);
  EEPROM.write(2, newPosition);
  EEPROM.commit();
  return newPosition;
}

int stopSud() {
  digitalWrite(D6, LOW);
  digitalWrite(D7, LOW);
  tSudMoving = false;
  int newPosition = whereInTheWorldIsTenda(
      tSudActBegin, tSudGoingUp, tSudPosition, topic_tende_status_sud);
  EEPROM.write(1, newPosition);
  EEPROM.write(3, newPosition);
  EEPROM.commit();
  return newPosition;
}

int posValidator(long position) {
  if (position > 100) {
    position = 100;
  }
  if (position < 0) {
    position = 0;
  }
  return int(position);
}

// connessione WiFi
// ---------------------------------------------------------------------------|
void setup_wifi() {
  delay(10);
  Serial.println("///////////////// - WiFi - /////////////////");
  Serial.print("Connessione a ");
  Serial.println(wifi_ssid);
  WiFi.mode(WIFI_STA);
  WiFi.begin(wifi_ssid, wifi_password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("Connesso");
  Serial.print("IP=");
  Serial.println(WiFi.localIP());
  Serial.println("////////////////////////////////////////////");
  Serial.println("");
}

// connessione MQTT
// ---------------------------------------------------------------------------|
void reconnect() {
  unsigned long disconnectMillis = millis();
  while (!client.connected()) {
    Serial.println("///////////////// - MQTT - /////////////////");
    Serial.println("Connessione...");

    if (client.connect(mqtt_id)) {
      // digitalWrite(D2,HIGH);
      Serial.println("Connesso");
      Serial.println("////////////////////////////////////////////");

      client.subscribe("roncello/esterno/est/set/#");
      client.subscribe("timer/1min");
    } else {
      if ((millis() - disconnectMillis) >= 30000) {
        emergencyProcedure();
      }
      Serial.print("Connessione fallita, rc=");
      Serial.println(client.state());
      Serial.println("Nuovo tentativo tra 5 secondi");
      delay(5000);
    }
  }
}

// callback MQTT
// ------------------------------------------------------------------------------|
void callback(char *topic, byte *payload, unsigned int length) {
  sTopic = topic;
  sPayload = "";

  lastMQTT = millis();
  Serial.print("Messaggio ricevuto [");
  Serial.print(topic);
  Serial.print("] ");
  for (int i = 0; i < length; i++) {
    Serial.print((char)payload[i]);
    sPayload += (char)payload[i];
  }
  Serial.println();
  Serial.println(String(sPayload));

  if (sTopic == topic_tende_set_nord) {
    if (tNordMoving) {
      tNordPosition = stopNord();
    }
    if (String(sPayload) != "STOP") {
      setNord(sPayload.toInt());
    }
  } else if (sTopic == topic_tende_set_sud) {
    if (tSudMoving) {
      tSudPosition = stopSud();
    }
    if (String(sPayload) != "STOP") {
      setSud(sPayload.toInt());
    }
  } else if (sTopic == topic_tende_set_group) {
    if (tSudMoving) {
      tSudPosition = stopSud();
    }
    if (tNordMoving) {
      tNordPosition = stopNord();
    }
    if (String(sPayload) != "STOP") {
      setSud(sPayload.toInt());
      setNord(sPayload.toInt());
    }
  } else if (sTopic == topic_upseconds_set) {
    upSeconds = sPayload.toInt();
    EEPROM.write(4, sPayload.toInt());
    EEPROM.commit();
  } else if (sTopic == topic_downseconds_set) {
    downSeconds = sPayload.toInt();
    EEPROM.write(5, sPayload.toInt());
    EEPROM.commit();
  }
}

void emergencyProcedure() {
  int tTargetNordPosition;
  int tTargetSudPosition;
  int tLastKnownNordPosition;
  int tLastKnownSudPosition;

  tTargetNordPosition = EEPROM.read(0);
  tTargetSudPosition = EEPROM.read(1);
  tLastKnownNordPosition = EEPROM.read(2);
  tLastKnownSudPosition = EEPROM.read(3);

  // find out if any of the previous states was != 0
  if (tTargetNordPosition != 0 || tTargetSudPosition != 0 ||
      tLastKnownNordPosition != 0 || tLastKnownSudPosition != 0) {
    // Perform procedure only if stationary (they might be closing already)
    if (!tSudMoving || !tNordMoving) {
      setNord(0);
      setSud(0);
    }
  }
}

// setup
// --------------------------------------------------------------------------------------|
void setup() {
  pinMode(D1, OUTPUT);
  pinMode(D2, OUTPUT);
  pinMode(D6, OUTPUT);
  pinMode(D7, OUTPUT);

  digitalWrite(D1, 0);
  digitalWrite(D2, 0);
  digitalWrite(D6, 0);
  digitalWrite(D7, 0);

  Serial.begin(115200);
  EEPROM.begin(10);
  setup_wifi();
  client.setServer(mqtt_server, 1883);
  client.setCallback(callback);
  tNordPosition = int(EEPROM.read(0));
  tSudPosition = int(EEPROM.read(1));

  tNordDuration = int(EEPROM.read(2));
  tSudDuration = int(EEPROM.read(3));

  upSeconds = EEPROM.read(4);
  downSeconds = EEPROM.read(5);

  httpUpdater.setup(&httpServer);
  httpServer.begin();
}

// loop
// ---------------------------------------------------------------------------------------|
void loop() {
  // controllo connessione mqtt
  // -------------------------------------------------------------|
  if (!client.connected()) {
    reconnect();
  }
  client.loop();
  httpServer.handleClient();

  if (tNordMoving == true) {
    if (millis() / 1000 > oldMillisNord) {
      whereInTheWorldIsTenda(tNordActBegin, tNordGoingUp, tNordPosition,
                             topic_tende_status_nord);
      oldMillisNord = millis() / 1000;
    }
    if ((millis() - tNordActBegin) > (tNordDuration)) {
      tNordPosition = stopNord();
    }
  }

  if (tSudMoving == true) {
    if (millis() / 1000 > oldMillisNord) {
      whereInTheWorldIsTenda(tSudActBegin, tSudGoingUp, tSudPosition,
                             topic_tende_status_sud);
      oldMillisSud = millis() / 1000;
    }
    if ((millis() - tSudActBegin) > (tSudDuration)) {
      tSudPosition = stopSud();
    }
  }

  if (lastMQTT < (millis() - 120000)) {
    emergencyProcedure();
  }
  delay(200);
}
