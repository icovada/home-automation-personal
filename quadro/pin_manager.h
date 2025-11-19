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

// External references to variables defined in quadro.ino
extern HAMqtt mqtt;

class Pin
{
  // Abstract Base Class for OutputPin and RemoteOutputPin
public:
  virtual void on() {}
  virtual void off() {}
  virtual void toggle() {}
  virtual ~Pin() {} //  virtual destructor
};

class OutputPin : public Pin
{
private:
  // Static map to associate HALight pointers with OutputPin instances
  static OutputPin *instances[3]; // Adjust size as needed
  static int instanceCount;

  static void registerInstanceLight(HALight *light, OutputPin *pin)
  {
    // Simple linear storage - could use a map in C++11
    for (int i = 0; i < instanceCount; i++)
    {
      if (instances[i] == pin)
        return; // Already registered
    }
    if (instanceCount < 16)
    {
      instances[instanceCount++] = pin;
    }
  }

  static void registerInstanceSwitch(HASwitch *haSwitch, OutputPin *pin)
  {
    // Simple linear storage - could use a map in C++11
    for (int i = 0; i < instanceCount; i++)
    {
      if (instances[i] == pin)
        return; // Already registered
    }
    if (instanceCount < 16)
    {
      instances[instanceCount++] = pin;
    }
  }

  static OutputPin *findInstanceLight(HALight *light)
  {
    // Find the instance that owns this HALight
    for (int i = 0; i < instanceCount; i++)
    {
      if (instances[i]->haLight == light)
      {
        return instances[i];
      }
    }
    return nullptr;
  }


  static OutputPin *findInstanceSwitch(HASwitch *haSwitch)
  {
    // Find the instance that owns this HASwitch
    for (int i = 0; i < instanceCount; i++)
    {
      if (instances[i]->haSwitch == haSwitch)
      {
        return instances[i];
      }
    }
    return nullptr;
  }

public:
  int pinNumber = -1;
  bool pinStatus = false;
  HALight *haLight;
  HASwitch *haSwitch;

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
    registerInstanceLight(haLight, this);
    haLight->onStateCommand(staticOnStateCommand);
    init();
  }

  OutputPin(int pinnumber, HASwitch *haSwitch)
      : pinNumber(pinnumber),
        haLight(nullptr),
        haSwitch(haSwitch)
  {
    registerInstanceSwitch(haSwitch, this);
    haSwitch->onCommand(staticOnStateCommandSwitch);
    init();
  }

  void init()
  {
    pinMode(pinNumber, OUTPUT);
    digitalWrite(pinNumber, 0);
    pinStatus = false;
  }

  // Static callback that retrieves the instance and calls the member function
  static void staticOnStateCommand(bool state, HALight *sender)
  {
    OutputPin *instance = findInstanceLight(sender);
    if (instance)
    {
      instance->onStateCommandLight(state, sender);
    }
  }

  static void staticOnStateCommandSwitch(bool state, HASwitch *sender)
  {
    OutputPin *instance = findInstanceSwitch(sender);
    if (instance)
    {
      instance->onStateCommandSwitch(state, sender);
    }
  }

  void onStateCommandLight(bool state, HALight *sender)
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

  void onStateCommandSwitch(bool state, HALight *sender)
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

  void onStateCommandSwitch(bool state, HASwitch *sender)
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
  int pinnumber = -1;

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
  HABinarySensor *sensorShort;
  HABinarySensor *sensorLong;
  Pin *pinShort;
  Pin *pinLong;
  String _name;

  String _shortId;
  String _longId;
  String _shortName;
  String _longName;

  PinManager()
      : sensorShort(nullptr),
        sensorLong(nullptr),
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
        sensorShort(nullptr),
        sensorLong(nullptr),
        pinShort(pinShort),
        pinLong(pinLong)
  {

    pinMode(inputPin, INPUT);

    // Pre-compute strings to ensure they persist
    _shortId = String(MQTT_NAME) + "_" + _name + "_short";
    _longId = String(MQTT_NAME) + "_" + _name + "_long";
    _shortName = String(MQTT_HUMAN_NAME) + " " + _name + " Short Press";
    _longName = String(MQTT_HUMAN_NAME) + " " + _name + " Long Press";

    sensorShort = new HABinarySensor(_shortId.c_str());
    sensorLong = new HABinarySensor(_longId.c_str());
    sensorShort->setName(_shortName.c_str());
    sensorLong->setName(_longName.c_str());
  }

  ~PinManager()
  {
    if (sensorShort != nullptr)
      delete sensorShort;
    if (sensorLong != nullptr)
      delete sensorLong;
    if (pinShort != nullptr)
      delete pinShort;
    if (pinLong != nullptr)
      delete pinLong;
  }

  void check()
  {
    // Skip if not initialized
    if (inputPin == -1 || sensorShort == nullptr || sensorLong == nullptr)
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
        if (sensorLong)
          sensorLong->setState(true);
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
          if (sensorShort)
            sensorShort->setState(true);
        }
        else
        {
          oldPinStatus = pinStatus;
          if (sensorLong)
            sensorLong->setState(false);
        }
      }
      else if (!lock && sensorShort && sensorShort->getCurrentState() && (millis() - activationTimer > 400))
      {
        sensorShort->setState(false);
      }
      debounce = millis();
    }
  }
};

#endif
