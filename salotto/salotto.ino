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

#include "EthernetIndustruino.h"
#include <Indio.h>
#include <PubSubClient.h>
#include <SPI.h>
#include <UC1701.h>

// Relè 1 2 3 4
// Letture 5 6
// Pulsanti A1 A2

int lcdBrightness = 255;
long lastReconnectAttempt = 0;

EthernetClient ethClient;
PubSubClient client;

String sTopic;
String sPayload;

String baseTopic = "/roncello/industruino/";

static UC1701 lcd;
#define mqtt_server "192.168.1.2"
#define subscribed_topic "roncello/industruino/set/#"
#define topic_lcd_brightness "roncello/salotto/set/industruino/lcdbrightness"
#define topic_riscaldamento_set "roncello/riscaldamento/set/generale"

// Enter a MAC address and IP address for your controller below.
byte mac[] = {0xDE, 0xAD, 0xBE, 0xEF, 0xFE, 0xED};
IPAddress ip(192, 168, 1, 3);

class InputPin {
public:
  InputPin() {}

  InputPin(int pin, bool islatching, bool isanalog, String name) {
    _pin = pin;
    _latching = islatching;
    _analog = isanalog;
    _debounce = millis();
    _name = name;
    if (isanalog) {
      Indio.analogReadMode(_pin, V10_p);
      _oldpinstatus = Indio.analogRead(_pin);
    } else {
      Indio.digitalMode(_pin, INPUT);
      _oldpinstatus = digitalRead(_pin);
    }
  }

  void Check() {
    if (millis() > _debounce + 30) {
      bool pinStatus;
      if (_analog) {
        if (Indio.analogRead(_pin) < 10) {
          pinStatus = 0;
        } else if (Indio.analogRead(_pin) > 80) {
          pinStatus = 1;
        }
      } else {
        pinStatus = Indio.digitalRead(_pin);
      }
      if (_latching) {
        if (pinStatus != _oldpinstatus) {
          _notifyChange("single");
          Serial.println("Single press");
          _oldpinstatus = pinStatus;
        }
      } else {
        if (pinStatus && !_oldpinstatus) { // if pressed and was not pressed
          _oldpinstatus = pinStatus;
          _lock = true;
          _activationTimer = millis();
        } else if (((millis() - _activationTimer) > 400) &&
                   _lock) { // If still pressed after 400 ms
          _lock = false;
          Serial.println("Long press");
          _notifyChange("long");
        } else if (!pinStatus && _oldpinstatus) { // if Let go
          if (_lock) {                            // if still in action
            _oldpinstatus = pinStatus;
            _lock = false;
            Serial.println("Single press");
            _notifyChange("single");
          } else {
            _oldpinstatus = pinStatus;
          }
        }
      }
      _debounce = millis();
    }
  }

protected:
  bool _lock;
  bool _oldpinstatus;
  bool _latching;
  bool _analog;
  unsigned long _debounce;
  int _pin;
  uint32_t _activationTimer;
  String _name;

  void _notifyChange(String event) {
    String topic = baseTopic + "event/" + _name;
    client.publish(topic.c_str(), event.c_str());
  }
};

void callback(char *topic, byte *payload, unsigned int length) {
  sTopic = topic;
  sPayload = "";

  for (int i = 0; i < length; i++) {
    sPayload += (char)payload[i];
  }
/*
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
  }*/
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
    client.subscribe(topic_riscaldamento_set);
    analogWrite(13, lcdBrightness);
  } else {
    Serial.print("failed, rc=");
    Serial.print(client.state());
    Serial.println(" try again in 10 seconds");
    lcd.setCursor(30, 0);
    lcd.print("FAILED       ");
  }
}

InputPin pinmanager[7];

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

  Indio.setADCResolution(12);

  // OUTPUTS
  Indio.digitalMode(1, OUTPUT); // Salotto ovest
  Indio.digitalMode(2, OUTPUT); // Salotto est
  Indio.digitalMode(3, OUTPUT); // Cucina
  Indio.digitalMode(4, OUTPUT); // Riscaldamento

  int pins[] = {5,6,7,1,2,3,4};
  bool latching[] = {0,0,0,0,0,0,0};
  bool isanalog[] = {0,0,0,1,1,1,1};
  String names[] = {"salotto/est",
                    "salotto/ovest",
                    "cucina/led/down",
                    "campanello",
                    "cucina/soffitto",
                    "salotto/lampade",
                    "cucina/led/up"};

  for (int i = 0; i < 7; i++){
    pinmanager[i] = InputPin(pins[i], latching[i], isanalog[i], names[i]);
  }

  client.setClient(ethClient);
  client.setServer(mqtt_server, 1883);
  client.setCallback(callback);
  lastReconnectAttempt = 0;

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
  } 

  for (int i = 0; i < 8; i++){
    pinmanager[i].Check();
  }
  
  client.loop();
}
