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
EthernetClient httpEthernetClient;
HADevice device("quadro", sizeof("quadro"));
HAMqtt mqtt(ethMqttClient, device);
String mqtt_server;

PinManager manager[2];
HABinarySensor shortsensor("short");
HABinarySensor longsensor("long");
HABinarySensor shortsensor2("short2");
HABinarySensor longsensor2("long2");

aREST rest = aREST();

String jsonConfig = "{\"net\":{\"ip\":[192,168,1,6],\"mask\":[255,255,255,0],\"gw\":[192,168,1,1],\"dns\":[192,168,1,1],\"mac\":\"001020304050\"},\"mqtt\":\"mqtt.in.tabbo.it\"}";
long lastReconnectAttempt = 0;

void saveJsonToEEPROM(char* json, int startAddr = 0) {
  int len = strlen(json);
  EEPROM.write(startAddr, len); // Store length first
  
  for (int i = 0; i < len; i++) {
    EEPROM.write(startAddr + 1 + i, json[i]);
  }
}

String readJsonFromEEPROM(int startAddr = 0) {
  int len = EEPROM.read(startAddr);
  char json[len + 1];
  
  for (int i = 0; i < len; i++) {
    json[i] = EEPROM.read(startAddr + 1 + i);
  }
  json[len] = '\0';
  
  return String(json);
}

void parseMacAddress(const char* macStr, byte* macArray) {
  for (int i = 0; i < 6; i++) {
    char hex[3] = {macStr[i*2], macStr[i*2 + 1], '\0'};
    macArray[i] = (byte)strtol(hex, NULL, 16);
  }
}

IPAddress parseIPAddress(JsonArrayConst address) {
  return IPAddress(
    address[0].as<int>(),
    address[1].as<int>(),
    address[2].as<int>(),
    address[3].as<int>()
  );
}

void onMqttMessage(char* topic, byte* payload, unsigned int length) {
  // This callback is called when message from MQTT broker is received.
  // Please note that you should always verify if the message's topic is the one you expect.
  // For example: if (memcmp(topic, "myCustomTopic") == 0) { ... }
  if (DEBUG_MODE) {
    Serial.print("Message arrived [");
    Serial.print(topic);
    Serial.print("] ");
    for (unsigned int i = 0; i < length; i++) {
      Serial.print((char)payload[i]);
    }
    Serial.println();
  }
}

void setup() {

  Serial.begin(9600);  
  Serial.println("Start");

  // Serial.println("Writing JSON to EEPROM");
  //  saveJsonToEEPROM(jsonConfig);

  String config = readJsonFromEEPROM();
  JsonDocument jsonConfig;

  Serial.println("Reading JSON from EEPROM...");
  String readJson = readJsonFromEEPROM();
  Serial.println(readJson);

  // Parse and use the JSON
  StaticJsonDocument<256> doc;
  DeserializationError error = deserializeJson(doc, readJson);
  
  if (error) {
    Serial.print("JSON parsing failed, getting address from DHCP, MAC: AE:AD:BE:EF:FE:FF");
    Serial.println(error.c_str());
    byte mac[] = { 0xAE, 0xAD, 0xBE, 0xEF, 0xFE, 0xFF };
    Ethernet.begin(mac);
  } else {
    byte mac[6];
    parseMacAddress(doc["net"]["mac"], mac);
    IPAddress ip = parseIPAddress(doc["net"]["ip"]);
    IPAddress mask = parseIPAddress(doc["net"]["mask"]);
    IPAddress gw = parseIPAddress(doc["net"]["gw"]);
    IPAddress dns = parseIPAddress(doc["net"]["dns"]);

    // start the Ethernet connection and the server:
    Ethernet.begin(mac, ip, dns, gw, mask);
    HttpClient http = HttpClient(httpEthernetClient, doc["remote"].as<String>(), 80);
  }

  rest.set_id("1");
  rest.set_name("quadro");

  shortsensor.setName("Controllino Test Short");
  longsensor.setName("Controllino Test Long");
  shortsensor2.setName("Controllino Test Short2");
  longsensor2.setName("Controllino Test Long2");

  manager[0] = PinManager(CONTROLLINO_A0, true, &shortsensor, &longsensor);
  manager[1] = PinManager(CONTROLLINO_A1, true, &shortsensor2, &longsensor2);

  // Register custom functions
  rest.function("reset", resetController);
  rest.function("replace_config", replaceConfig);
  rest.function("set_digital", setDigitalPin);
  rest.function("get_digital", getDigitalPin);
  rest.function("set_analog", setAnalogPin);
  rest.function("get_analog", getAnalogPin);
  ethServer.begin();

  mqtt.onMessage(onMqttMessage);
  mqtt.begin(doc["mqtt"].as<const char*>());

  Serial.println("End");
  lastReconnectAttempt = 0;

}

void loop() {
  httpRestClient = ethServer.available();
  rest.handle(httpRestClient);

  mqtt.loop();

  for (int i=0;i<2;i++){
    manager[i].check();
  }
}
