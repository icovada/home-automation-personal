#ifndef PIN_MANAGER_H
#define PIN_MANAGER_H

#include <Arduino.h>
#include <ArduinoHA.h>

// External references to variables defined in quadro.ino
extern HAMqtt mqtt;
extern String baseTopic;

class Pin {
  // Abstract Base Class for OutputPin and RemoteOutputPin
public:
  virtual void on() {}
  virtual void off() {}
  virtual void toggle() {}
  virtual ~Pin() {}  //  virtual destructor
};

class OutputPin : public Pin {
protected:
  int pinNumber = -1;
  bool pinStatus = false;
  String _name = "";

public:
  OutputPin(int pinnumber, String name) {
    pinNumber = pinnumber;
    _name = name;
    digitalWrite(pinNumber, LOW);
    pinStatus = false;
  }

  void on() override {
    digitalWrite(pinNumber, HIGH);
    pinStatus = true;
    String topic = baseTopic + "status/" + _name;
    mqtt.publish(topic.c_str(), "ON");
  }

  void off() override {
    digitalWrite(pinNumber, LOW);
    pinStatus = false;
    String topic = baseTopic + "status/" + _name;
    mqtt.publish(topic.c_str(), "OFF");
  }

  void toggle() override {
    if (pinStatus) {
      off();
    } else {
      on();
    }
  }
};

class RemoteOutputPin : public Pin {
protected:
  String _remote_host = "";
  int _pinnumber = -1;

public:
  RemoteOutputPin(String remote_host, int pinnumber) {
    _remote_host = remote_host;
    _pinnumber = pinnumber;
  }

  void on() override {}      // TODO
  void off() override {}     // TODO
  void toggle() override {}  // TODO
};

class FakeOutputPin : public Pin {
public:
  FakeOutputPin() {}

  void on() override {}
  void off() override {}
  void toggle() override {}
};

class PinManager {
public:
  int inputPin = -1;
  bool lock = false;
  bool oldPinStatus = false;
  bool analog = false;
  unsigned long debounce = millis();
  uint32_t activationTimer = 0;
  HABinarySensor* sensorShort;
  HABinarySensor* sensorLong;
  String _name;
  String _shortId;
  String _longId;
  String _shortName;
  String _longName;

  PinManager()
    : sensorShort(nullptr), sensorLong(nullptr), _name("") {}  // Default constructor

  PinManager(int inPin, bool isAnalog, String name)
    : inputPin(inPin),
      analog(isAnalog),
      _name(name),
      sensorShort(nullptr),
      sensorLong(nullptr) {
    pinMode(inputPin, INPUT);

    // Pre-compute strings to ensure they persist
    _shortId = _name + "_short";
    _longId = _name + "_long";
    _shortName = _name + " Short Press";
    _longName = _name + " Long Press";
  }

  void initSensors() {
    sensorShort = new HABinarySensor(_shortId.c_str());
    sensorLong = new HABinarySensor(_longId.c_str());
    sensorShort->setName(_shortName.c_str());
    sensorLong->setName(_longName.c_str());
  }

  ~PinManager() {
    if (sensorShort != nullptr) delete sensorShort;
    if (sensorLong != nullptr) delete sensorLong;
  }

  void check() {
    // Skip if not initialized
    if (inputPin == -1 || sensorShort == nullptr || sensorLong == nullptr) {
      return;
    }

    if (millis() > debounce + 30) {
      bool pinStatus;
      if (analog) {
        if (analogRead(inputPin) < 10) {
          pinStatus = 0;
        } else if (analogRead(inputPin) > 80) {
          pinStatus = 1;
        }
      } else {
        pinStatus = digitalRead(inputPin);
      }

      if (pinStatus && !oldPinStatus) {  // if pressed and was not pressed
        oldPinStatus = pinStatus;
        lock = true;
        activationTimer = millis();
      } else if (((millis() - activationTimer) > 400) && lock) {  // If still pressed after 400 ms
        lock = false;
        Serial.println("Long press");
        if (sensorLong) sensorLong->setState(true);
      } else if (!pinStatus && oldPinStatus) {  // if Let go
        if (lock) {                             // if still in action
          oldPinStatus = pinStatus;
          lock = false;
          Serial.println("Single press");
          if (sensorShort) sensorShort->setState(true);
        } else {
          oldPinStatus = pinStatus;
          if (sensorLong) sensorLong->setState(false);
        }
      } else if (!lock && sensorShort && sensorShort->getCurrentState() && (millis() - activationTimer > 400)) {
        sensorShort->setState(false);
      }
      debounce = millis();
    }
  }

  void _notifyChange(String event) {
  }
};

#endif
