String baseTopic = "pippo/";

class Pin{
  // Abstract Base Class for OutputPin and RemoteOutputPin
  public:
    virtual void on() {}
    virtual void off() {}
    virtual void toggle() {}
    virtual ~Pin() {} //  virtual destructor
};

class OutputPin : public Pin {
  protected:
    int _pinnumber = -1;
    bool _pinstatus = false;
    String _name = "";

  public:
    OutputPin(int pinnumber, String name) {
      _pinnumber = pinnumber;
      _name = name;
      digitalWrite(_pinnumber, LOW);
      _pinstatus = false;
    }

    void on() override {
      digitalWrite(_pinnumber, HIGH);
      _pinstatus = true;
      String topic = baseTopic + "status/" + _name;
      mqtt.publish(topic.c_str(), "ON");
    }

    void off() override {
      digitalWrite(_pinnumber, LOW);
      _pinstatus = false;
      String topic = baseTopic + "status/" + _name;
      mqtt.publish(topic.c_str(), "OFF");
    }

    void toggle() override {
      if (_pinstatus) {
        off();
      } else {
        on();
      }
    }
  };

class RemoteOutputPin : public Pin{
  protected:
    String _remote_host = "";
    int _pinnumber = -1;

  public:
    RemoteOutputPin(String remote_host, int pinnumber) {
      _remote_host = remote_host;
      _pinnumber = pinnumber;
    }

    void on() override {}// TODO
    void off() override {} // TODO
    void toggle() override {} // TODO
};

class PinManager{
  // Abstract Base Class for LocalPinManager and RemotePinManager
  public:
    int _pin = -1;
    bool _lock = false;
    bool _oldpinstatus = false;
    bool _analog = false;
    unsigned long _debounce = millis();
    uint32_t _activationTimer = 0;
    HABinarySensor short;
    HABinarySensor long;
    String _name = "fake";
    Pin _outpin;
    
    void check() {
      if (_pin == -1) {
        // this instance does not have a pin input
        // such as a relay whose button is on another PLC
        return;
      }
      if (millis() > _debounce + 30){
        bool pinStatus;
        if (_analog) {
          if (analogRead(_pin) < 10) {
            pinStatus = 0;
          } else if (analogRead(_pin) > 80) {
            pinStatus = 1;
          }
        } else {
          pinStatus = digitalRead(_pin);
        }

        if (pinStatus && !_oldpinstatus) { // if pressed and was not pressed
          _oldpinstatus = pinStatus;
          _lock = true;
          _activationTimer = millis();
        } else if (((millis() - _activationTimer) > 400) && _lock) { // If still pressed after 400 ms
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

    void _notifyChange(String event){
    }
};

class LocalPinManager : public PinManager{
  public:
    LocalPinManager() {}

    LocalPinManager(String name, int outpin) 
      : LocalPinManager(-1, false, name, outpin) {} // out, no in

    LocalPinManager(int pin, bool isanalog, String name)
      : LocalPinManager(pin, isanalog, name, -1) {}

    LocalPinManager(int pin, bool isanalog, String name, int outpin) { // inout
      _pin = pin;
      _analog = isanalog;
      _name = name;

      if (outpin != -1){
        _outpin = OutputPin(outpin, _name);
      }

      if (_pin != -1){
        pinMode(_pin, INPUT);
      }
    }
};