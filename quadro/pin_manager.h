#ifndef PIN_MANAGER_H
#define PIN_MANAGER_H

#include <Arduino.h>
#include <ArduinoHA.h>

// External references to variables defined in quadro.ino
extern HAMqtt mqtt;

String baseTopic = "pippo/";

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

  PinManager() : sensorShort(nullptr), sensorLong(nullptr) {}  // Default constructor

  PinManager(int inPin, bool isAnalog, HABinarySensor* inSensorShort, HABinarySensor* inSensorLong) {  // inout
    inputPin = inPin;
    analog = isAnalog;
    sensorShort = inSensorShort;
    sensorLong = inSensorLong;

    pinMode(inputPin, INPUT);
  }

  void check() {
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
      } else if (!lock && sensorShort && sensorShort->getCurrentState()) {
        sensorShort->setState(false);
      }
      debounce = millis();
    }
  }

  void _notifyChange(String event) {
  }
};

#endif
