#ifndef CONTROLLINO_COMMON_H
#define CONTROLLINO_COMMON_H

#include <Controllino.h>
#include <Ethernet.h>
#include <ArduinoJson.h>
#include <avr/wdt.h>
#include <ArduinoHA.h>
#include "eeprom_stuff.h"
#include "pin_manager.h"

// Forward declaration
void saveJsonToEEPROM(char *json, int startAddr = 0);

// Returns available RAM in bytes
int freeRam()
{
  extern int __heap_start, *__brkval;
  int v;
  return (int)&v - (__brkval == 0 ? (int)&__heap_start : (int)__brkval);
}

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

// Initialize MQTT from JSON config
void initMQTT(HADevice &device, HAMqtt &mqtt, StaticJsonDocument<256> &doc, char *mqtt_server, size_t mqtt_server_size, const char *humanName)
{
  device.setName(humanName);
  device.enableSharedAvailability();
  device.enableLastWill();

  if (doc.containsKey("mqtt") && doc["mqtt"].is<const char *>())
  {
    strlcpy(mqtt_server, doc["mqtt"].as<const char *>(), mqtt_server_size);
    mqtt.begin(mqtt_server);
    Serial.println("MQTT initialized");
  }
  else
  {
    Serial.println("WARNING: MQTT config missing, MQTT not initialized");
  }
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

// Standard loop operations for MQTT and pin managers
template <int MANAGER_COUNT>
void standardLoopOperations(HAMqtt &mqtt, char *mqtt_server, PinManager *manager[MANAGER_COUNT])
{
  // Reconnect to MQTT if connection lost
  if (!mqtt.isConnected() && mqtt_server[0] != '\0')
  {
    mqtt.begin(mqtt_server);
  }

  mqtt.loop();

  for (int i = 0; i < MANAGER_COUNT; i++)
  {
    manager[i]->check();
  }

  checkPendingStateSave();
  wdt_reset();
}

#endif // CONTROLLINO_COMMON_H
