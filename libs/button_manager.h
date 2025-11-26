#ifndef PIN_MANAGER_H
#define PIN_MANAGER_H

#ifndef MQTT_NAME
#define MQTT_NAME "controllino_quadro"
#endif

#ifndef MQTT_HUMAN_NAME
#define MQTT_HUMAN_NAME "Controllino Quadro"
#endif

#include <Arduino.h>
#include <ArduinoHA.h>
#include <Ethernet.h>
#include <JC_Button.h> // https://github.com/JChristensen/JC_Button
#include "eeprom_stuff.h"

enum buttonStateMachine
{
  UNPRESSED,
  PRESSED,
  LONG_PRESSED,
  RELEASED
};

// External references to variables defined in quadro.ino
extern HAMqtt mqtt;

class Pin
{
  // Abstract Base Class for OutputPin and RemoteOutputPin
public:
  int pinNumber = -1;
  enum PinType
  {
    TYPE_OUTPUT,
    TYPE_REMOTE
  };
  virtual PinType getType() const = 0; // Pure virtual - subclasses must implement

  virtual void on() {}
  virtual void off() {}
  virtual void toggle() {}
  virtual ~Pin() {} //  virtual destructor
};

class OutputPin : public Pin
{
private:
  // Reference to global pin registry
  inline static Pin **globalPins = nullptr;
  inline static int *globalPinCount = nullptr;

  static OutputPin *findInstanceByLight(HALight *haObject)
  {
    if (!globalPins || !globalPinCount)
      return nullptr;

    for (int i = 0; i < *globalPinCount; i++)
    {
      // Check type first to ensure safe casting
      if (globalPins[i] && globalPins[i]->getType() == Pin::TYPE_OUTPUT)
      {
        OutputPin *outPin = static_cast<OutputPin *>(globalPins[i]);
        if (outPin->haLight == haObject)
        {
          return outPin;
        }
      }
    }
    return nullptr;
  }

  static OutputPin *findInstanceBySwitch(HASwitch *haObject)
  {
    if (!globalPins || !globalPinCount)
      return nullptr;

    for (int i = 0; i < *globalPinCount; i++)
    {
      // Check type first to ensure safe casting
      if (globalPins[i] && globalPins[i]->getType() == Pin::TYPE_OUTPUT)
      {
        OutputPin *outPin = static_cast<OutputPin *>(globalPins[i]);
        if (outPin->haSwitch == haObject)
        {
          return outPin;
        }
      }
    }
    return nullptr;
  }

public:
  bool pinStatus = false;
  HALight *haLight;
  HASwitch *haSwitch;

  // Setup global registry reference
  // we need this for findInstanceBySwitch/ByLight
  static void setupGlobalRegistry(Pin **pins, int *count)
  {
    globalPins = pins;
    globalPinCount = count;
  }

  // Build state bitmap and save to EEPROM
  static void saveAllPinStates()
  {
    if (!globalPins || !globalPinCount)
      return;

    byte stateData = 0;
    for (int i = 0; i < *globalPinCount; i++)
    {
      if (globalPins[i] && globalPins[i]->getType() == Pin::TYPE_OUTPUT)
      {
        OutputPin *outPin = static_cast<OutputPin *>(globalPins[i]);
        if (outPin->pinStatus && i < 8) // Only support up to 8 pins in one byte
        {
          stateData |= (1 << i);
        }
      }
    }
    saveStateToEEPROM(stateData);
  }

  // Restore state for a specific pin index
  static void restorePinState(int index, byte stateData)
  {
    if (!globalPins || !globalPinCount || index >= *globalPinCount)
      return;

    if (globalPins[index] && globalPins[index]->getType() == Pin::TYPE_OUTPUT)
    {
      OutputPin *outPin = static_cast<OutputPin *>(globalPins[index]);
      bool shouldBeOn = (stateData & (1 << index)) != 0;

      if (shouldBeOn)
      {
        outPin->on();
      }
      else
      {
        outPin->off();
      }
    }
  }

  // Implement getType() for OutputPin
  PinType getType() const override { return Pin::TYPE_OUTPUT; }

  OutputPin()
      : haLight(nullptr),
        haSwitch(nullptr) {}

  OutputPin(int pinnumber)
      : haLight(nullptr),
        haSwitch(nullptr)
  {
    pinNumber = pinnumber;
  }

  OutputPin(int pinnumber, HALight *haLight)
      : haLight(haLight),
        haSwitch(nullptr)
  {
    pinNumber = pinnumber;
    haLight->setCurrentState(false);
    haLight->onStateCommand(staticOnStateLight);
    init();
  }

  OutputPin(int pinnumber, HASwitch *haSwitch)
      : haLight(nullptr),
        haSwitch(haSwitch)
  {
    pinNumber = pinnumber;
    haSwitch->setCurrentState(false);
    haSwitch->onCommand(staticOnStateSwitch);
    init();
  }

  void init()
  {
    pinMode(pinNumber, OUTPUT);
    digitalWrite(pinNumber, 0);
    pinStatus = false;
  }

  /*
  These static methods staticOnState* (classmethods in Python) exist because we
  cannot pass a normal class function to haSwitch/haLight->onCommand.

  Since we no longer know "who" we are, we call findInstanceBy* to loop through
  the global Pin array to find "us" out by the sender and then call
  mqttSetter to change the Pin status
  */
  static void staticOnStateLight(bool state, HALight *sender)
  {
    OutputPin *instance = findInstanceByLight(sender);
    if (instance)
    {
      instance->mqttSetter(state);
    }
  }

  static void staticOnStateSwitch(bool state, HASwitch *sender)
  {
    OutputPin *instance = findInstanceBySwitch(sender);
    if (instance)
    {
      instance->mqttSetter(state);
    }
  }

  void mqttSetter(bool state)
  {
    if (state)
    {
      on();
    }
    else
    {
      off();
    }
  }

  void on() override
  {
    if (haLight)
    {
      haLight->setState(true);
    }
    if (haSwitch)
    {
      haSwitch->setState(true);
    }
    digitalWrite(pinNumber, HIGH);
    pinStatus = true;
    saveAllPinStates(); // Save state to EEPROM
  }

  void off() override
  {
    if (haLight)
    {
      haLight->setState(false);
    }
    if (haSwitch)
    {
      haSwitch->setState(false);
    }
    digitalWrite(pinNumber, LOW);
    pinStatus = false;
    saveAllPinStates(); // Save state to EEPROM
  }

  void toggle() override
  {
    Serial.println("Pin toggle");
    Serial.println(pinNumber);
    if (pinStatus)
    {
      off();
    }
    else
    {
      on();
    }
  }
};

class RemoteOutputPin : public Pin
{
public:
  String remoteHost;
  int remotePort = 80;

  // Implement getType() for RemoteOutputPin
  PinType getType() const override { return Pin::TYPE_REMOTE; }

  RemoteOutputPin(char *host, int port, int pinnumber)
      : remoteHost(host),
        remotePort(port)
  {
    pinNumber = pinnumber;
  }

  void on() override
  {
    toggle();
  }
  void off() override
  {
    toggle();
  }
  void toggle() override
  {
    Serial.println("Remote pin toggle");

    // Create a fresh EthernetClient for this request
    EthernetClient ethClient;

    // Ensure client is disconnected before attempting connection
    if (ethClient.connected())
    {
      ethClient.stop();
      delay(50);
    }

    // Construct the full path
    String path = "/toggle?params=" + String(pinNumber);

    // Connect to the remote server with retry
    int retries = 0;
    while (retries < 3 && !ethClient.connect(remoteHost.c_str(), remotePort))
    {
      ethClient.stop();
      delay(50);
      retries++;
    }

    if (ethClient.connected())
    {
      // Build complete request as single string to ensure it's sent in one packet
      String request = "GET " + path + "\r\n";

      // Send the entire request at once
      ethClient.print(request);
      ethClient.flush(); // Ensure data is sent
      ethClient.stop();
    }
    else
    {
      ethClient.stop();
    }
  }
};

class ButtonManager
{
public:
  int inputPin = -1;
  bool lock = false;
  bool oldPinStatus = false;
  bool analog = false;
  unsigned long debounce = millis();
  uint32_t activationTimer = 0;
  uint32_t lastReleaseTime = 0;
  uint8_t clickCount = 0;
  HADeviceTrigger *shortPressTrigger;
  HADeviceTrigger *longPressTrigger;
  HADeviceTrigger *doublePressTrigger;
  HADeviceTrigger *triplePressTrigger;
  Pin *pinShort;
  Pin *pinLong;
  Pin *pinDouble;
  Pin *pinTriple;
  char _triggerId[32]; // Fixed buffer instead of String

  ButtonManager()
      : shortPressTrigger(nullptr),
        longPressTrigger(nullptr),
        doublePressTrigger(nullptr),
        triplePressTrigger(nullptr),
        pinShort(nullptr),
        pinLong(nullptr),
        pinDouble(nullptr),
        pinTriple(nullptr)
  {
    _triggerId[0] = '\0';
  }

  ButtonManager(int inPin, bool isAnalog, const char *name)
      : ButtonManager(inPin, isAnalog, name, nullptr, nullptr, nullptr, nullptr) {}

  ButtonManager(int inPin, bool isAnalog, const char *name, Pin *pinShort)
      : ButtonManager(inPin, isAnalog, name, pinShort, nullptr, nullptr, nullptr) {}

  ButtonManager(int inPin, bool isAnalog, const char *name, Pin *pinShort, Pin *pinLong)
      : ButtonManager(inPin, isAnalog, name, pinShort, pinLong, nullptr, nullptr) {}

  ButtonManager(int inPin, bool isAnalog, const char *name, Pin *pinShort, Pin *pinLong, Pin *pinDouble)
      : ButtonManager(inPin, isAnalog, name, pinShort, pinLong, pinDouble, nullptr) {}

  ButtonManager(int inPin, bool isAnalog, const char *name, Pin *pinShort, Pin *pinLong, Pin *pinDouble, Pin *pinTriple)
      : inputPin(inPin),
        analog(isAnalog),
        shortPressTrigger(nullptr),
        longPressTrigger(nullptr),
        doublePressTrigger(nullptr),
        triplePressTrigger(nullptr),
        pinShort(pinShort),
        pinLong(pinLong),
        pinDouble(pinDouble),
        pinTriple(pinTriple)
  {
    pinMode(inputPin, INPUT);

    // Build trigger ID into fixed buffer
    strlcpy(_triggerId, MQTT_NAME, sizeof(_triggerId));
    strlcat(_triggerId, "_", sizeof(_triggerId));
    strlcat(_triggerId, name, sizeof(_triggerId));

    shortPressTrigger = new HADeviceTrigger(HADeviceTrigger::ButtonShortPressType, _triggerId);
    longPressTrigger = new HADeviceTrigger(HADeviceTrigger::ButtonLongPressType, _triggerId);
    doublePressTrigger = new HADeviceTrigger(HADeviceTrigger::ButtonDoublePressType, _triggerId);
    triplePressTrigger = new HADeviceTrigger(HADeviceTrigger::ButtonTriplePressType, _triggerId);
  }

  ~ButtonManager()
  {
    if (shortPressTrigger != nullptr)
      delete shortPressTrigger;
    if (longPressTrigger != nullptr)
      delete longPressTrigger;
    if (doublePressTrigger != nullptr)
      delete doublePressTrigger;
    if (triplePressTrigger != nullptr)
      delete triplePressTrigger;
  }

  void check()
  {
    // Skip if not initialized
    if (inputPin == -1 || shortPressTrigger == nullptr || longPressTrigger == nullptr)
    {
      return;
    }

    if (millis() > debounce + 30)
    {
      bool pinStatus;
      if (analog)
      {
        if (analogRead(inputPin) < 10)
        {
          pinStatus = 0;
        }
        else if (analogRead(inputPin) > 80)
        {
          pinStatus = 1;
        }
      }
      else
      {
        pinStatus = digitalRead(inputPin);
      }

      if (pinStatus && !oldPinStatus)
      { // Button just pressed
        oldPinStatus = pinStatus;
        lock = true;
        activationTimer = millis();
      }
      else if (((millis() - activationTimer) > 400) && lock)
      { // If still pressed after 400 ms - LONG PRESS
        lock = false;
        clickCount = 0; // Reset click count
        lastReleaseTime = 0;
        Serial.println("Long press");
        if (pinLong)
          pinLong->toggle();
        if (longPressTrigger)
          longPressTrigger->trigger();
      }
      else if (!pinStatus && oldPinStatus)
      { // Button just released
        if (lock)
        { // Was a valid short press (not already triggered as long press)
          oldPinStatus = pinStatus;
          lock = false;

          // Check if this is part of a multi-click sequence
          if (clickCount == 0 || (millis() - lastReleaseTime) <= 400)
          {
            // Within multi-click window
            clickCount++;
            lastReleaseTime = millis();
          }
          else
          {
            // Too much time passed, reset count
            clickCount = 1;
            lastReleaseTime = millis();
          }
        }
        else
        {
          oldPinStatus = pinStatus;
        }
      }

      // Check if we should trigger based on click count timeout
      if (clickCount > 0 && (millis() - lastReleaseTime) > 400)
      {
        // Multi-click timeout expired, trigger appropriate action
        if (clickCount == 1)
        {
          Serial.println("Single press");
          if (pinShort)
            pinShort->toggle();
          if (shortPressTrigger)
            shortPressTrigger->trigger();
        }
        else if (clickCount == 2)
        {
          Serial.println("Double press");
          if (pinDouble)
            pinDouble->toggle();
          if (doublePressTrigger)
            doublePressTrigger->trigger();
        }
        else if (clickCount >= 3)
        {
          Serial.println("Triple press");
          if (pinTriple)
            pinTriple->toggle();
          if (triplePressTrigger)
            triplePressTrigger->trigger();
        }

        clickCount = 0; // Reset for next sequence
        lastReleaseTime = 0;
      }

      debounce = millis();
    }
  }
};

#endif
