#define MQTT_NAME "controllino_quadro"
#define MQTT_HUMAN_NAME "Controllino Quadro"

#include <Controllino.h>
#include <SPI.h>
#include <Ethernet.h>
#include <aREST.h>
#include <EEPROM.h>
#include <ArduinoJson.h> // https://arduinojson.org/
#include <avr/wdt.h>
#include <PubSubClient.h>
#include <ArduinoHA.h> // https://github.com/dawidchyrzynski/arduino-home-assistant/
#include <ArduinoHttpClient.h>
#include "pin_manager.h"
#include "rest_functions.h"

#define DEBUG_MODE 1

EthernetServer ethServer(80);
EthernetClient ethMqttClient;
EthernetClient httpRestClient;
HADevice device("quadro");
HAMqtt mqtt(ethMqttClient, device);
String mqtt_server;
String remoteHttpHost = "";

aREST rest = aREST();

// Global registry of PinManagers
#define MANAGER_COUNT 2
#define PIN_COUNT 3

PinManager *manager[MANAGER_COUNT];
static Pin *pins[PIN_COUNT];
static int pinCount = 0;

void saveJsonToEEPROM(char *json, int startAddr = 0)
{
  int len = strlen(json);

  // Validate length fits in a byte and EEPROM
  if (len > 250 || len == 0)
  {
    Serial.println("ERROR: JSON too large or empty for EEPROM");
    return;
  }

  EEPROM.write(startAddr, (byte)len);

  // Calculate checksum while writing
  byte checksum = 0;
  for (int i = 0; i < len; i++)
  {
    EEPROM.write(startAddr + 1 + i, json[i]);
    checksum ^= json[i];

    // Reset watchdog every 32 bytes to prevent timeout during long writes
    if (i % 32 == 0)
    {
      wdt_reset();
    }
  }

  // Store checksum at end
  EEPROM.write(startAddr + 1 + len, checksum);
  wdt_reset(); // Final reset before completing
  Serial.println("Saved to EEPROM with checksum");
}

String readJsonFromEEPROM(int startAddr = 0)
{
  int len = EEPROM.read(startAddr);

  // Limit max length to prevent stack overflow, check for uninitialized EEPROM
  if (len > 250 || len == 0 || len == 0xFF)
  {
    Serial.println("Invalid EEPROM length");
    return "";
  }

  String result = "";
  result.reserve(len + 1);

  // Read data and calculate checksum
  byte checksum = 0;
  for (int i = 0; i < len; i++)
  {
    char c = (char)EEPROM.read(startAddr + 1 + i);
    result += c;
    checksum ^= c;
  }

  // Verify checksum
  byte storedChecksum = EEPROM.read(startAddr + 1 + len);
  if (checksum != storedChecksum)
  {
    Serial.println("ERROR: EEPROM checksum mismatch - data corrupted");
    return "";
  }

  return result;
}

void parseMacAddress(const char *macStr, byte *macArray)
{
  // Validate MAC string length (should be at least 12 characters for AABBCCDDEEFF)
  if (!macStr || strlen(macStr) < 12)
  {
    Serial.println("ERROR: Invalid MAC address length");
    // Set default MAC on error
    byte defaultMac[] = {0xAE, 0xAD, 0xBE, 0xEF, 0xFE, 0xFF};
    memcpy(macArray, defaultMac, 6);
    return;
  }

  for (int i = 0; i < 6; i++)
  {
    char hex[3] = {macStr[i * 2], macStr[i * 2 + 1], '\0'};
    macArray[i] = (byte)strtol(hex, NULL, 16);
  }
}

IPAddress parseIPAddress(JsonArrayConst address)
{
  return IPAddress(
      address[0].as<int>(),
      address[1].as<int>(),
      address[2].as<int>(),
      address[3].as<int>());
}

void setup()
{
  wdt_disable(); // Disable watchdog timer immediately
  Serial.begin(115200);
  Serial.println("Start");

  Serial.println("Reading JSON from EEPROM...");
  String readJson = readJsonFromEEPROM();
  Serial.println(readJson);

  // Parse and use the JSON
  StaticJsonDocument<256> doc;
  DeserializationError error = deserializeJson(doc, readJson);

  if (error)
  {
    Serial.print("JSON parsing failed, getting address from DHCP, MAC: AE:AD:BE:EF:FE:FF");
    Serial.println(error.c_str());
    byte mac[] = {0xAE, 0xAD, 0xBE, 0xEF, 0xFE, 0xFF};
    Ethernet.begin(mac);
  }
  else
  {
    // Validate required JSON fields exist
    if (!doc.containsKey("net") || !doc["net"].containsKey("mac") ||
        !doc["net"].containsKey("ip") || !doc["net"].containsKey("mask") ||
        !doc["net"].containsKey("gw") || !doc["net"].containsKey("dns"))
    {
      Serial.println("ERROR: JSON missing required network fields, falling back to DHCP");
      byte mac[] = {0xAE, 0xAD, 0xBE, 0xEF, 0xFE, 0xFF};
      Ethernet.begin(mac);
    }
    else
    {
      byte mac[6];
      parseMacAddress(doc["net"]["mac"], mac);
      IPAddress ip = parseIPAddress(doc["net"]["ip"]);
      IPAddress mask = parseIPAddress(doc["net"]["mask"]);
      IPAddress gw = parseIPAddress(doc["net"]["gw"]);
      IPAddress dns = parseIPAddress(doc["net"]["dns"]);

      // start the Ethernet connection and the server:
      Ethernet.begin(mac, ip, dns, gw, mask);

      if (doc.containsKey("remote"))
      {
        remoteHttpHost = doc["remote"].as<String>();
        Serial.println("Init remote http to " + remoteHttpHost);
      }

      Serial.print("Local IP: ");
      Serial.println(Ethernet.localIP());
    }
  }

  device.setName(MQTT_HUMAN_NAME);

  rest.set_id("1");
  rest.set_name("quadro");

  // Register custom functions
  rest.function("reset", resetController);
  rest.function("replace_config", replaceConfig);
  ethServer.begin();

  // Declare HALights and HASwitches
  HALight *light = new HALight("testlight");
  light->setName("Test Light");

  HASwitch *haswitch = new HASwitch("switch1");
  haswitch->setName("sw Test 1");

  // Hand off "pins" to OutputPin class
  // this is a global value for the entire class
  OutputPin::setupGlobalRegistry(pins, &pinCount);

  pins[0] = new OutputPin(CONTROLLINO_D0, light);
  pins[1] = new OutputPin(CONTROLLINO_D1, haswitch);
  pins[2] = new RemoteOutputPin(remoteHttpHost, 80, 2);
  pinCount = 3;

  // Initialize PinManagers before MQTT
  manager[0] = new PinManager(CONTROLLINO_A0, true, "Test1", pins[0], pins[1]);
  manager[1] = new PinManager(CONTROLLINO_A1, true, "Test2", pins[2]);

  device.enableSharedAvailability();
  device.enableLastWill();

  // Validate MQTT config exists before initializing
  if (doc.containsKey("mqtt") && doc["mqtt"].is<const char *>())
  {
    mqtt.begin(doc["mqtt"].as<const char *>());
    Serial.println("MQTT initialized");
  }
  else
  {
    Serial.println("WARNING: MQTT config missing, MQTT not initialized");
  }

  // Enable watchdog. Reset PLC if not reset every 500ms
  wdt_enable(WDTO_4S);
  Serial.println("End");
}

void loop()
{
  httpRestClient = ethServer.available();
  rest.handle(httpRestClient);

  mqtt.loop();

  for (int i = 0; i < MANAGER_COUNT; i++)
  {
    manager[i]->check();
  }

  // Tell watchdog to chill
  wdt_reset();
}
