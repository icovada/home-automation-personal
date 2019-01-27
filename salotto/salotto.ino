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

#include <EthernetIndustruino.h>
#include <Indio.h>
#include <PubSubClient.h>
#include <SPI.h>
#include <UC1701.h>

// Relè 1 2 3 4
// Letture 5 6
// Pulsanti A1 A2

bool pinUno = 0;
bool pinUnoNew = 0;
bool pinDue = 0;
bool pinDueNew = 0;
bool pinTre = 0;
bool pinTreNew = 0;

bool pinA1 = 0;
bool pinA2 = 0;
bool pinA3 = 0;
bool pinA4 = 0;

bool pinCinque = 0;
bool pinSei = 0;
bool pinSette = 0;
bool pinCinqueNew = 0;
bool pinSeiNew = 0;
bool pinSetteNew = 0;

int lcdBrightness = 255;
long lastReconnectAttempt = 0;

EthernetClient ethClient;
PubSubClient client;

String sTopic;
String sPayload;

static UC1701 lcd;

#define mqtt_server "192.168.1.2"

#define subscribed_topic "roncello/salotto/set/#"
#define tempOvest "roncello/esterno/ovest/status/temperatura"
#define tempEst "roncello/esterno/est/status/temperatura"

#define topic_salottoEst_set "roncello/salotto/set/luci/salotto/est"
#define topic_salottoEst_status "roncello/salotto/status/luci/salotto/est"

#define topic_salottoOvest_set "roncello/salotto/set/luci/salotto/ovest"
#define topic_salottoOvest_status "roncello/salotto/status/luci/salotto/ovest"

#define topic_cucina_soffitto_set "roncello/salotto/set/luci/cucina/soffitto"
#define topic_cucina_soffitto_status                                           \
  "roncello/salotto/status/luci/cucina/soffitto"

#define topic_cucina_led "roncello/cucina/set/led"

#define topic_campanello_casa "roncello/salotto/status/campanello"
#define topic_salottoSonus "roncello/salotto/set/campanello"

#define topic_lcd_brightness "roncello/salotto/set/industruino/lcdbrightness"

#define topic_riscaldamento_set "roncello/riscaldamento/set/generale"
#define topic_riscaldamento_status "roncello/riscaldamento/status/generale"

// Enter a MAC address and IP address for your controller below.
byte mac[] = {0xDE, 0xAD, 0xBE, 0xEF, 0xFE, 0xED};
IPAddress ip(192, 168, 1, 3);

bool mqttDo(String payload, String pubTopic, int pinNumber, bool pinStatus) {
  pinStatus = !pinStatus;
  if (payload == "ON") {
    Indio.digitalWrite(pinNumber, HIGH);
    client.publish(pubTopic.c_str(), String("ON").c_str(), true);
  } else {
    Indio.digitalWrite(pinNumber, LOW);
    client.publish(pubTopic.c_str(), String("OFF").c_str(), true);
  }
  return pinStatus;
}

void callback(char *topic, byte *payload, unsigned int length) {
  sTopic = topic;
  sPayload = "";

  for (int i = 0; i < length; i++) {
    Serial.print((char)payload[i]);
    sPayload += (char)payload[i];
  }

  if (sTopic == topic_salottoEst_set) {
    pinUno = mqttDo(sPayload, topic_salottoEst_status, 1, pinUno);
  }
  if (sTopic == topic_salottoOvest_set) {
    pinDue = mqttDo(sPayload, topic_salottoOvest_status, 2, pinDue);
  }
  if (sTopic == topic_cucina_soffitto_set) {
    pinTre = mqttDo(sPayload, topic_cucina_soffitto_status, 3, pinTre);
  }
  if (sTopic == topic_riscaldamento_set) {
    mqttDo(sPayload, topic_riscaldamento_status, 4, 0);
    lcd.setCursor(42, 4);
    lcd.print(String(sPayload).c_str());
    lcd.print(" ");
  }
  if (sTopic == tempEst) {
    lcd.setCursor(42, 2);
    lcd.print(String(sPayload).c_str());
  }
  if (sTopic == tempOvest) {
    lcd.setCursor(42, 3);
    lcd.print(String(sPayload).c_str());
  }
  if (sTopic == topic_lcd_brightness) {
    lcdBrightness = map(sPayload.toInt(), 0, 100, 255, 0);
    analogWrite(13, lcdBrightness);
  }
}

bool physicalToggle(bool old, int pinNumber, String topic) {
  old = !old;
  if (old) {
    Indio.digitalWrite(pinNumber, HIGH);
    client.publish(topic.c_str(), String("ON").c_str(), true);
    Serial.println("ON");
  } else {
    Indio.digitalWrite(pinNumber, LOW);
    client.publish(topic.c_str(), String("OFF").c_str(), true);
    Serial.println("OFF");
  }
  return old;
}

boolean reconnect() {
  // Loop until we're reconnected
  if (client.connect("industruino")) {
    lcd.setCursor(30, 0);
    lcd.print("Connected    ");
    Serial.println("connected");
    client.subscribe(subscribed_topic);
    client.subscribe(tempOvest);
    client.subscribe(tempEst);
    client.subscribe(topic_riscaldamento_set);
    lcd.setCursor(0, 2);
    lcd.print("Est:          °c");
    lcd.setCursor(0, 3);
    lcd.print("Ovest:        °c");
    lcd.setCursor(0, 4);
    lcd.print("Risc:           ");
    analogWrite(13, lcdBrightness);
  } else {
    Serial.print("failed, rc=");
    Serial.print(client.state());
    Serial.println(" try again in 10 seconds");
    lcd.setCursor(30, 0);
    lcd.print("FAILED       ");
    lcd.setCursor(0, 2);
    lcd.print("                ");
    lcd.setCursor(0, 3);
    lcd.print("                ");
    lcd.setCursor(0, 4);
    lcd.print("                ");
  }
}

void setup() {
  lcd.begin();
  lcd.setCursor(0, 0);
  lcd.print("Booting...");
  pinMode(10, OUTPUT); // change this to 53 on a mega
  pinMode(4, OUTPUT);  // change this to 53 on a mega
  pinMode(6, OUTPUT);  // change this to 53 on a mega
  pinMode(13, OUTPUT);
  digitalWrite(10, HIGH);
  digitalWrite(6, HIGH);
  digitalWrite(4, HIGH);
  digitalWrite(13, LOW);
  Serial.begin(9600);
  Serial.println("Vai");

  // Open serial communications and wait for port to open:
  Ethernet.begin(mac, ip);

  delay(5000);
  Serial.println("Ci siamo");
  Indio.digitalMode(1, OUTPUT); // Salotto ovest
  Indio.digitalMode(2, OUTPUT); // Salotto est
  Indio.digitalMode(3, OUTPUT); // Cucina
  Indio.digitalMode(4, OUTPUT); // Riscaldamento
  Indio.digitalMode(5, INPUT);  // Salotto est
  Indio.digitalMode(6, INPUT);  // Salotto ovest
  Indio.digitalMode(7, INPUT);  // Led cucina DOWN

  Indio.setADCResolution(12);
  Indio.analogReadMode(1, V10_p); // Campanello casa
  Indio.analogReadMode(2, V10_p); // Cucina
  Indio.analogReadMode(3, V10_p); // Sonoff salotto
  Indio.analogReadMode(4, V10_p); // Led cucina UP

  client.setClient(ethClient);
  client.setServer(mqtt_server, 1883);
  client.setCallback(callback);
  lastReconnectAttempt = 0;

  pinCinque = Indio.digitalRead(5);
  pinSei = Indio.digitalRead(6);
  pinSette = Indio.digitalRead(7);

  Indio.digitalWrite(1, LOW);
  Indio.digitalWrite(2, LOW);
  Indio.digitalWrite(3, LOW);
  Indio.digitalWrite(4, LOW);
}

void loop() {
  if (!client.connected()) {
    long now = millis();
    if (now - lastReconnectAttempt > 10000) {
      digitalWrite(13, LOW);
      lcd.clear();
      lcd.setCursor(0, 0);
      lcd.print("MQTT:");
      lcd.setCursor(30, 0);
      lcd.print("Connecting...");
      Serial.print("Attempting MQTT connection...");
      lastReconnectAttempt = now;
      // Attempt to connect
      if (reconnect()) {
        lastReconnectAttempt = 0;
      }
    }
  } else {
    client.loop();
  }
  pinCinqueNew = Indio.digitalRead(5);
  pinSeiNew = Indio.digitalRead(6);
  pinSetteNew = Indio.digitalRead(7);

  if ((pinCinqueNew == 1) && (pinCinqueNew != pinCinque)) {
    pinUnoNew = physicalToggle(pinUno, 1, topic_salottoEst_status);
  }

  if ((pinSeiNew == 1) && (pinSeiNew != pinSei)) {
    pinDueNew = physicalToggle(pinDue, 2, topic_salottoOvest_status);
  }

  if ((pinSetteNew == 1) && (pinSetteNew != pinSette)) {
    client.publish(topic_cucina_led, String("DOWN").c_str());
  }

  if ((Indio.analogRead(1) > 80) && (pinA1 == 0)) {
    client.publish(topic_campanello_casa, String("ON").c_str());
    pinA1 = 1;
  }

  if ((Indio.analogRead(1) < 10) && (pinA1 == 1)) {
    client.publish(topic_campanello_casa, String("OFF").c_str(), true);
    pinA1 = 0;
  }

  if ((Indio.analogRead(2) > 80) && (pinA2 == 0)) {
    pinTreNew = physicalToggle(pinTre, 3, topic_cucina_soffitto_status);
    pinA2 = 1;
  }
  if ((Indio.analogRead(2) < 10) && (pinA2 == 1)) {
    pinA2 = 0;
  }

  if ((Indio.analogRead(3) > 80) && (pinA3 == 0)) {
    client.publish(topic_salottoSonus, String("Toggle").c_str());
    pinA3 = 1;
  }
  if ((Indio.analogRead(3) < 10) && (pinA3 == 1)) {
    pinA3 = 0;
  }

  if ((Indio.analogRead(4) > 80) && (pinA4 == 0)) {
    client.publish(topic_cucina_led, String("UP").c_str());
    pinA4 = 1;
  }
  if ((Indio.analogRead(4) < 10) && (pinA4 == 1)) {
    pinA4 = 0;
  }

  pinUno = pinUnoNew;
  pinDue = pinDueNew;
  pinTre = pinTreNew;
  pinCinque = pinCinqueNew;
  pinSei = pinSeiNew;
  pinSette = pinSetteNew;
  client.loop();
}
