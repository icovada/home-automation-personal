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
  enum PinType { TYPE_OUTPUT, TYPE_REMOTE };
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

  // Implement getType() for OutputPin
  PinType getType() const override { return Pin::TYPE_OUTPUT; }

  OutputPin()
      : haLight(nullptr),
        haSwitch(nullptr) {}

  OutputPin(int pinnumber)
      : pinNumber(pinnumber),
        haLight(nullptr),
        haSwitch(nullptr) {}

  OutputPin(int pinnumber, HALight *haLight)
      : pinNumber(pinnumber),
        haLight(haLight),
        haSwitch(nullptr)
  {
    haLight->setCurrentState(false);
    haLight->onStateCommand(staticOnStateLight);
    init();
  }

  OutputPin(int pinnumber, HASwitch *haSwitch)
      : pinNumber(pinnumber),
        haLight(nullptr),
        haSwitch(haSwitch)
  {
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

  RemoteOutputPin(String host, int port, int pinnumber)
      : remoteHost(host),
        remotePort(port),
        pinnumber(pinnumber) {}

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
    String path = "/led?params=" + String(pinnumber) + ",2";

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

class PinManager
{
public:
  int inputPin = -1;
  bool lock = false;
  bool oldPinStatus = false;
  bool analog = false;
  unsigned long debounce = millis();
  uint32_t activationTimer = 0;
  HADeviceTrigger *shortPressTrigger;
  HADeviceTrigger *longPressTrigger;
  Pin *pinShort;
  Pin *pinLong;
  String _name;
  String _triggerId;

  PinManager()
      : shortPressTrigger(nullptr),
        longPressTrigger(nullptr),
        pinShort(nullptr),
        pinLong(nullptr),
        _name("") {}

  PinManager(int inPin, bool isAnalog, String name)
      : PinManager(inPin, isAnalog, name, nullptr, nullptr) {}

  PinManager(int inPin, bool isAnalog, String name, Pin *pinShort)
      : PinManager(inPin, isAnalog, name, pinShort, nullptr) {}

  PinManager(int inPin, bool isAnalog, String name, Pin *pinShort, Pin *pinLong)
      : inputPin(inPin),
        analog(isAnalog),
        _name(name),
        shortPressTrigger(nullptr),
        longPressTrigger(nullptr),
        pinShort(pinShort),
        pinLong(pinLong)
  {

    pinMode(inputPin, INPUT);

    // Pre-compute strings to ensure they persist
    _triggerId = String(MQTT_NAME) + "_" + _name;

    shortPressTrigger = new HADeviceTrigger(HADeviceTrigger::ButtonShortPressType, _triggerId.c_str());
    longPressTrigger = new HADeviceTrigger(HADeviceTrigger::ButtonLongPressType, _triggerId.c_str());
  }

  ~PinManager()
  {
    if (shortPressTrigger != nullptr)
      delete shortPressTrigger;
    if (longPressTrigger != nullptr)
      delete longPressTrigger;
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
      { // if pressed and was not pressed
        oldPinStatus = pinStatus;
        lock = true;
        activationTimer = millis();
      }
      else if (((millis() - activationTimer) > 400) && lock)
      { // If still pressed after 400 ms
        lock = false;
        Serial.println("Long press");
        if (pinLong)
          pinLong->toggle();
        if (longPressTrigger)
          longPressTrigger->trigger();
      }
      else if (!pinStatus && oldPinStatus)
      { // if Let go
        if (lock)
        { // if still in action
          oldPinStatus = pinStatus;
          lock = false;
          Serial.println("Single press");
          if (pinShort)
            pinShort->toggle();
          if (shortPressTrigger)
            shortPressTrigger->trigger();
        }
        else
        {
          oldPinStatus = pinStatus;
        }
      }
      debounce = millis();
    }
  }
};

#endif
