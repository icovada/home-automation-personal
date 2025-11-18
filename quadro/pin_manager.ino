class PinManager{
  protected:
    int _pin = 0;
    bool _lock = false;
    bool _oldpinstatus = false;
    bool _analog = false;
    unsigned long _debounce = 50;
    uint32_t _activationTimer = 0;
    String _name = "fake";

  public:
  PinManager() {}

  PinManager(int pin, bool isanalog, String name){
    _pin = pin;
    _analog = isanalog;
    _debounce = millis();
    _name = name;

    pinMode(pin, INPUT);
  }

  void check() {
    if (_pin == 0) {
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

  void _notifyChange(String event) {
  }

  // String getName() { return _name; };



};