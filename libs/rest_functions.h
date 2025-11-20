#include <ArduinoJson.h> // https://arduinojson.org/

#ifndef DEBUG_MODE
#define DEBUG_MODE 0
#endif

// Forward declaration of function defined in quadro.ino
void saveJsonToEEPROM(char *json, int startAddr = 0);

int resetController(String command)
{
  wdt_enable(WDTO_15MS);
  while (1)
  {
  }
  return 1; // Never reached
}

int replaceConfig(String data)
{
  JsonDocument jsonConfig;

  if (DEBUG_MODE)
  {
    Serial.println("Parsing incoming JSON");
    Serial.println(data);
  }

  // Replace URL-encoded spaces with actual spaces
  data.replace("+", " ");
  data.replace("%20", " ");
  data.replace("%22", "\"");

  if (DEBUG_MODE)
  {
    Serial.println("URL-decoded data:");
    Serial.println(data);
  }

  StaticJsonDocument<256> doc;
  DeserializationError error = deserializeJson(doc, data);
  if (error)
  {
    Serial.println("New config not valid");
    Serial.println(error.c_str());
    return 1;
  }
  Serial.println("Config valid, writing to EEPROM...");
  saveJsonToEEPROM((char *)data.c_str());
  return 0;
}
