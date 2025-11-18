#include <ArduinoJson.h>  // https://arduinojson.org/

#ifndef DEBUG_MODE
#define DEBUG_MODE 0
#endif

int resetController(String command) {
  wdt_enable(WDTO_15MS);
  while (1) {}
  return 1;  // Never reached
}

// Custom function to control digital pins with MQTT publishing
int setDigitalPin(String command) {
  if (DEBUG_MODE) {
    Serial.println("Called setDigitalPin");
    Serial.println("Received HTTP request, with data:");
    Serial.println(command);
  }
  int commaIndex = command.indexOf(',');
  int pin = command.substring(0, commaIndex).toInt();
  int value = command.substring(commaIndex + 1).toInt();

  if (DEBUG_MODE) {
    // Log the change
    Serial.print("Setting digital pin ");
    Serial.print(pin);
    Serial.print(" to ");
    Serial.println(value);
  }

  // Actually change the pin
  pinMode(pin, OUTPUT);
  digitalWrite(pin, value);

  // Publish to MQTT
  String topic = "quadro/digital/" + String(pin);
  String payload = String(value);
  mqtt.publish(topic.c_str(), payload.c_str());

  return 1;  // Success
}

// Custom function to read digital pins with MQTT publishing
int getDigitalPin(String command) {
  if (DEBUG_MODE) {
    Serial.println("Called setDigitalPin");
    Serial.println("Received HTTP request, with data:");
    Serial.println(command);
  }

  int pin = command.toInt();

  pinMode(pin, INPUT);
  int value = digitalRead(pin);

  if (DEBUG_MODE) {
    Serial.print("Reading digital pin ");
    Serial.print(pin);
    Serial.print(": ");
    Serial.println(value);
  }

  // Publish to MQTT
  String topic = "quadro/digital/" + String(pin);
  String payload = String(value);
  mqtt.publish(topic.c_str(), payload.c_str());

  return value;
}

// Custom function to control analog pins (PWM) with MQTT publishing
int setAnalogPin(String command) {
  // Parse pin and value from command (format: "pin,value")
  int commaIndex = command.indexOf(',');
  if (commaIndex == -1) {
    Serial.println("Error: Invalid format. Use pin,value");
    return 0;
  }

  int pin = command.substring(0, commaIndex).toInt();
  int value = command.substring(commaIndex + 1).toInt();

  // Constrain value to valid PWM range
  value = constrain(value, 0, 255);

  // Log the change
  Serial.print("Setting analog pin ");
  Serial.print(pin);
  Serial.print(" to ");
  Serial.println(value);

  // Actually change the pin
  pinMode(pin, OUTPUT);
  analogWrite(pin, value);

  // Publish to MQTT
  String topic = "quadro/analog/" + String(pin);
  String payload = String(value);
  mqtt.publish(topic.c_str(), payload.c_str());
  Serial.print("Published to MQTT: ");
  Serial.print(topic);
  Serial.print(" = ");
  Serial.println(payload);

  return 1;  // Success
}

// Custom function to read analog pins with MQTT publishing
int getAnalogPin(String command) {
  int pin = command.toInt();

  int value = analogRead(pin);

  Serial.print("Reading analog pin ");
  Serial.print(pin);
  Serial.print(": ");
  Serial.println(value);

  // Publish to MQTT
  String topic = "quadro/analog/" + String(pin);
  String payload = String(value);
  mqtt.publish(topic.c_str(), payload.c_str());

  return value;
}

int replaceConfig(String data) {
  JsonDocument jsonConfig;

  if (DEBUG_MODE) {
    Serial.println("Parsing incoming JSON");
    Serial.println(data);
  }

  // Replace URL-encoded spaces with actual spaces
  data.replace("+", " ");
  data.replace("%20", " ");
  data.replace("%22", "\"");

  if (DEBUG_MODE) {
    Serial.println("URL-decoded data:");
    Serial.println(data);
  }

  StaticJsonDocument<256> doc;
  DeserializationError error = deserializeJson(doc, data);
  if (error) {
    Serial.println("New config not valid");
    Serial.println(error.c_str());
    return 1;
  }
  Serial.println("Config valid, writing to EEPROM...");
  saveJsonToEEPROM((char*)data.c_str());
  return 0;
}
