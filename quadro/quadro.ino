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
PinManager *manager[2];
static Pin *pins[3];
static int pinCount = 0;


void saveJsonToEEPROM(char *json, int startAddr = 0)
{
  int len = strlen(json);
  EEPROM.write(startAddr, len); // Store length first

  for (int i = 0; i < len; i++)
  {
    EEPROM.write(startAddr + 1 + i, json[i]);
  }
}

String readJsonFromEEPROM(int startAddr = 0)
{
  int len = EEPROM.read(startAddr);

  // Limit max length to prevent stack overflow
  if (len > 250 || len == 0)
  {
    return "";
  }

  String result = "";
  result.reserve(len + 1);

  for (int i = 0; i < len; i++)
  {
    result += (char)EEPROM.read(startAddr + 1 + i);
  }

  return result;
}

void parseMacAddress(const char *macStr, byte *macArray)
{
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
  Serial.begin(9600);
  Serial.println("Start");

  String config = readJsonFromEEPROM();
  JsonDocument jsonConfig;

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
    byte mac[6];
    parseMacAddress(doc["net"]["mac"], mac);
    IPAddress ip = parseIPAddress(doc["net"]["ip"]);
    IPAddress mask = parseIPAddress(doc["net"]["mask"]);
    IPAddress gw = parseIPAddress(doc["net"]["gw"]);
    IPAddress dns = parseIPAddress(doc["net"]["dns"]);

    // start the Ethernet connection and the server:
    Ethernet.begin(mac, ip, dns, gw, mask);
    remoteHttpHost = doc["remote"].as<String>();
    Serial.println("Init remote http to " + remoteHttpHost);
    Serial.print("Local IP: ");
    Serial.println(Ethernet.localIP());
  }

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

  mqtt.begin(doc["mqtt"].as<const char *>());

  Serial.println("End");
}

void loop()
{
  httpRestClient = ethServer.available();
  rest.handle(httpRestClient);

  mqtt.loop();

  for (int i = 0; i < 2; i++)
  {
    manager[i]->check();
  }
}
