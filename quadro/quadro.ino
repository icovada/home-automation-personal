#define MQTT_NAME "controllino_quadro"
#define MQTT_HUMAN_NAME "Controllino Quadro"

#include <Controllino.h>
#include <Ethernet.h>
#include <aREST.h>
#include <ArduinoJson.h> // https://arduinojson.org/
#include <avr/wdt.h>
#include <ArduinoHA.h> // https://github.com/dawidchyrzynski/arduino-home-assistant/
#include "eeprom_stuff.h"
#include "pin_manager.h"

#define DEBUG_MODE 1
#define MANAGER_COUNT 2
#define PIN_COUNT 3

#include "rest_functions.h"

EthernetServer ethServer(80);
EthernetClient ethMqttClient;
EthernetClient httpRestClient;
HADevice device("quadro");
HAMqtt mqtt(ethMqttClient, device);
String mqtt_server;
String remoteHttpHost = "";

aREST rest = aREST();

// Global registry of PinManagers
PinManager *manager[MANAGER_COUNT];
static Pin *pins[PIN_COUNT];
static int pinCount = 0;

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
  rest.function("toggle", togglePin);
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
    mqtt_server = doc["mqtt"].as<const char *>();
    mqtt.begin(mqtt_server.c_str());
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

  // Reconnect to MQTT if connection lost
  if (!mqtt.isConnected() && mqtt_server.length() > 0)
  {
    mqtt.begin(mqtt_server.c_str());
  }

  mqtt.loop();

  for (int i = 0; i < MANAGER_COUNT; i++)
  {
    manager[i]->check();
  }

  // Tell watchdog to chill
  wdt_reset();
}
