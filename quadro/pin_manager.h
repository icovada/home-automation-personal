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
    digitalWrite(pinNumber, LOW);
    pinStatus = false;
  }

  OutputPin(int pinnumber, HASwitch *haSwitch)
      : pinNumber(pinnumber),
        haLight(nullptr),
        haSwitch(haSwitch)
  {
    digitalWrite(pinNumber, LOW);
    pinStatus = false;
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
    HttpClient client(ethClient, remoteHost, remotePort);
    client.setTimeout(1000); // 1 second timeout
    client.setHttpResponseTimeout(1000);

    // Construct the full path
    String path = "/led?params=" + String(pinnumber) + ",2";
    Serial.print("Requesting: http://");
    Serial.print(remoteHost);
    Serial.print(":");
    Serial.print(remotePort);
    Serial.println(path);

    int err = client.get(path);

    if (err == 0)
    {
      Serial.println("Request sent successfully, waiting for response...");

      // read the status code and body of the response
      int statusCode = client.responseStatusCode();
      String response = client.responseBody();

      Serial.print("Status code: ");
      Serial.println(statusCode);
      Serial.print("Response: ");
      Serial.println(response);

      // Stop the client to free resources
      client.stop();
    }
    else
    {
      Serial.print("HTTP GET failed, error: ");
      Serial.println(err);
      Serial.print("Client connected: ");
      Serial.println(client.connected());
      client.stop();
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
