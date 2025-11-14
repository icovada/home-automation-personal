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

#include "Ethernet.h"
#include <Indio.h>
#include <PubSubClient.h>
#include <SPI.h>

EthernetClient ethClient;
PubSubClient client;
int lastReconnectAttempt;

String baseTopic = "/roncello/quadro/";
#define mqtt_server "mqtt.in.tabbo.it"
#define subscribed_topic "/roncello/quadro/set/#"

// Enter a MAC address and IP address for your controller below.
byte mac[] = {0xDE, 0xAD, 0xBE, 0xEF, 0xFE, 0xFF};
IPAddress ip(192, 168, 1, 6);
IPAddress gateway(192, 168, 1, 1);
IPAddress subnet(255, 255, 255, 0);

class PinManager {
  public:
    PinManager() {}

    PinManager(String name, int outpin) {
      _name = name;
      _outpin = OutputPin(outpin);
      _debounce = 4294967296; // never trigger
    }

    PinManager(int pin, bool isanalog, String name) {
      _pin = pin;
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

    PinManager(int pin, bool isanalog, String name, int outpin) {
      _pin = pin;
      _analog = isanalog;
      _debounce = millis();
      _name = name;
      _outpin = OutputPin(outpin);
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
            _notifyChange("letgo");
          }
        }
        _debounce = millis();
      }
    }

    String getName() { return _name; }

    void mqttManager(String payload) {
      if (payload == "ON") {
        _outpin.On(_name);
      } else if (payload == "OFF") {
        _outpin.Off(_name);
      } else {
        _outpin.Toggle(_name);
      }
    }

  protected:
    bool _lock;
    bool _oldpinstatus;
    bool _analog;
    unsigned long _debounce;
    int _pin;
    uint32_t _activationTimer;
    String _name;

    class OutputPin {
    public:
      OutputPin() {}
      OutputPin(int pinnumber) {
        _pinnumber = pinnumber;
        Indio.digitalMode(_pinnumber, OUTPUT);
        Indio.digitalWrite(_pinnumber, LOW);
        _pinstatus = false;
      }

      void On(String name) {
        Indio.digitalWrite(_pinnumber, HIGH);
        _pinstatus = true;
        String topic = baseTopic + "status/" + name;
        client.publish(topic.c_str(), "ON");
      }

      void Off(String name) {
        Indio.digitalWrite(_pinnumber, LOW);
        _pinstatus = false;
        String topic = baseTopic + "status/" + name;
        client.publish(topic.c_str(), "OFF");
      }

      void Toggle(String name) {
        if (_pinstatus) {
          Off(name);
        } else {
          On(name);
        }
      }

    protected:
      int _pinnumber;
      bool _pinstatus;
    };

    OutputPin _outpin;

    void _notifyChange(String event) {
      if (client.connected()) {
        String topic = baseTopic + "event/" + _name;
        client.publish(topic.c_str(), event.c_str());
      } else {
        _outpin.Toggle(_name);
      }
    }
};

PinManager pinmanager[7];

void callback(char *topic, byte *payload, unsigned int length) {
  String sTopic = topic;
  sTopic = sTopic.substring(26);
  String sPayload = "";

  for (int i = 0; i < length; i++) {
    sPayload += (char)payload[i];
  }

  Serial.println(sTopic);

  for (int i = 0; i < 7; i++) {
    Serial.println(pinmanager[i].getName());
    if (sTopic == pinmanager[i].getName()) {
      pinmanager[i].mqttManager(sPayload);
      break;
    }
  }
}

boolean reconnect() {
  // Loop until we're reconnected
  if (client.connect("quadro")) {
    Serial.println("connected");
    client.subscribe(subscribed_topic);
  } else {
    Serial.print("failed, rc=");
    Serial.print(client.state());
  }
}

void setup() {
  Serial.begin(9600);
  Serial.println("Begin Setup");

  // Open serial communications and wait for port to open:
  Ethernet.begin(mac, ip, gateway, gateway, subnet);

  Serial.println("Ci siamo");

  client.setClient(ethClient);
  client.setServer(mqtt_server, 1883);
  client.setCallback(callback);
  lastReconnectAttempt = 0;

  pinmanager[0] = PinManager(5, 0, "salotto/est", 2);
  pinmanager[1] = PinManager(6, 0, "salotto/ovest", 1);
  pinmanager[2] = PinManager(7, 0, "cucina/led/down");
  pinmanager[3] = PinManager(1, 1, "campanello");
  pinmanager[4] = PinManager(2, 1, "cucina/soffitto", 3);
  pinmanager[5] = PinManager(3, 1, "salotto/lampade");
  pinmanager[6] = PinManager(4, 1, "cucina/led/up");

  Serial.println("End Setup");

}

void loop() {
  if (!client.connected()) {
    long now = millis();
    if (now - lastReconnectAttempt > 10000) {
      digitalWrite(13, LOW);
      Serial.print("Connecting");
      lastReconnectAttempt = now;
      // Attempt to connect
      if (reconnect()) {
        lastReconnectAttempt = 0;
      }
    }
  }

  for (int i = 0; i < 7; i++) {
    pinmanager[i].Check();
  }

  client.loop();
}
