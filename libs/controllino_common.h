#include <Controllino.h>
#include <Ethernet.h>
#include <ArduinoJson.h>
#include <avr/wdt.h>
#include <ArduinoHA.h>
#include "eeprom_stuff.h"
#include "any_common.h"
#include "pin_manager.h"

// Forward declaration
void saveJsonToEEPROM(char *json, int startAddr = 0);

// Initialize serial and RS485 communication
void initSerial()
{
  Serial.begin(115200);
  Serial.println("Start");

  // Initialize RS485 serial for Modbus
  Serial3.begin(38400, SERIAL_8N2);
  Controllino_RS485Init(38400);
  Controllino_RS485RxEnable();
}

// Parse network configuration from JSON and initialize Ethernet
// Returns the parsed JSON document for further use
// remoteHttpHost is optional - pass nullptr if not needed
bool initNetworkFromEEPROM(StaticJsonDocument<256> &doc, char *remoteHttpHost = nullptr, size_t remoteHostSize = 0)
{
  Serial.println("Reading JSON from EEPROM...");
  String readJson = readJsonFromEEPROM();
  Serial.println(readJson);

  DeserializationError error = deserializeJson(doc, readJson);

  if (error)
  {
    Serial.print("JSON parsing failed, getting address from DHCP, MAC: AE:AD:BE:EF:FE:FF");
    Serial.println(error.c_str());
    byte mac[] = {0xAE, 0xAD, 0xBE, 0xEF, 0xFE, 0xFF};
    Ethernet.begin(mac);
    return false;
  }

  // Validate required JSON fields exist
  if (!doc.containsKey("net") || !doc["net"].containsKey("mac") ||
      !doc["net"].containsKey("ip") || !doc["net"].containsKey("mask") ||
      !doc["net"].containsKey("gw") || !doc["net"].containsKey("dns"))
  {
    Serial.println("ERROR: JSON missing required network fields, falling back to DHCP");
    byte mac[] = {0xAE, 0xAD, 0xBE, 0xEF, 0xFE, 0xFF};
    Ethernet.begin(mac);
    return false;
  }

  byte mac[6];
  parseMacAddress(doc["net"]["mac"], mac);
  IPAddress ip = parseIPAddress(doc["net"]["ip"]);
  IPAddress mask = parseIPAddress(doc["net"]["mask"]);
  IPAddress gw = parseIPAddress(doc["net"]["gw"]);
  IPAddress dns = parseIPAddress(doc["net"]["dns"]);

  // Copy remote host if provided and available in config
  if (remoteHttpHost && remoteHostSize > 0 && doc.containsKey("remote"))
  {
    strlcpy(remoteHttpHost, doc["remote"] | "", remoteHostSize);
  }

  Ethernet.begin(mac, ip, dns, gw, mask);

  Serial.print("Local IP: ");
  Serial.println(Ethernet.localIP());
  return true;
}



// Restore pin states from EEPROM and apply to OutputPins
void restoreAndApplyPinStates(Pin **pins, int count)
{
  Serial.println("Restoring light states from EEPROM...");
  byte savedStates = restoreStateFromEEPROM();

  for (int i = 0; i < count; i++)
  {
    bool shouldBeOn = (savedStates & (1 << i)) != 0;
    if (shouldBeOn && pins[i])
    {
      pins[i]->on();
      Serial.print("Pin ");
      Serial.print(i);
      Serial.println(" restored to ON");
    }
  }
}

