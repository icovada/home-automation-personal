#include "Controllino.h"
#include "Ethernet.h"
#include "SPI.h"

// Enter a MAC address for your controller below.
// Newer Ethernet shields have a MAC address printed on a sticker on the shield
byte mac[] = {0xDE, 0xAD, 0xBE, 0xEF, 0xFE, 0xEF};
// Set the static IP address to use if the DHCP fails to assign
IPAddress ip(192, 168, 1, 4);

class PinManager {
public:
  PinManager() {}

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

PinManager pinmanager[9];

void callback(char *topic, byte *payload, unsigned int length) {
  String sTopic = topic;
  sTopic = sTopic.substring(26);
  String sPayload = "";

  for (int i = 0; i < length; i++) {
    sPayload += (char)payload[i];
  }

  Serial.println(sTopic);

  for (int i = 0; i < 9; i++) {
    Serial.println(pinmanager[i].getName());
    if (sTopic == pinmanager[i].getName()) {
      pinmanager[i].mqttManager(sPayload);
      break;
    }
  }
}

boolean reconnect() {
  // Loop until we're reconnected
  if (client.connect("industruino")) {
    lcd.setCursor(30, 0);
    lcd.print("Connected    ");
    Serial.println("connected");
    client.subscribe(subscribed_topic);
    analogWrite(13, lcdBrightness);
  } else {
    Serial.print("failed, rc=");
    Serial.print(client.state());
    lcd.setCursor(30, 0);
    lcd.print("FAILED       ");
  }
}

void setup() {
  pinMode(CONTROLLINO_A0, INPUT);

  Serial.begin(9600);
  // start the Ethernet connection:
  if (Ethernet.begin(mac) == 0) {
    Serial.println("Failed to configure Ethernet using DHCP");
    // try to congifure using IP address instead of DHCP:
    Ethernet.begin(mac, ip);
  }
  // give the Ethernet shield a second to initialize:
  delay(5000);
  Serial.println("Ci siamo");

  client.setClient(ethClient);
  client.setServer(mqtt_server, 1883);
  client.setCallback(callback);
  lastReconnectAttempt = 0;

  pinmanager[0] = PinManager(CONTROLLINO_A0, "antibagno", 2);
  pinmanager[1] = PinManager(CONTROLLINO_A1, "bagno/luce", 1);
  pinmanager[2] = PinManager(CONTROLLINO_A2, "bagno/specchio");
  pinmanager[3] = PinManager(CONTROLLINO_A3, "disimpegno");
  pinmanager[4] = PinManager(CONTROLLINO_A4, "camera/luce", 3);
  pinmanager[5] = PinManager(CONTROLLINO_A5, "camera/lampada");
  pinmanager[6] = PinManager(CONTROLLINO_A6, "sgabuzzino/luce");
  pinmanager[7] = PinManager(CONTROLLINO_A7, "sgabuzzino/ventola");
  pinmanager[8] = PinManager(CONTROLLINO_A8, "studio/luce");
  pinmanager[9] = PinManager(CONTROLLINO_A9, "studio/lampada");
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
      Serial.print("Connecting");
      lastReconnectAttempt = now;
      // Attempt to connect
      if (reconnect()) {
        lastReconnectAttempt = 0;
      }
    }
  }

  for (int i = 0; i < 9; i++) {
    pinmanager[i].Check();
  }

  if (Indio.digitalRead(4) && ((millis() - riscaldamentoShutdown) > 7200000)) {
    Indio.digitalWrite(4, LOW);
  }

  client.loop();
}