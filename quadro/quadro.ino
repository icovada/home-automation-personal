#include <Controllino.h>
#include <SPI.h>
#include <Ethernet.h>
#include <aREST.h>
#include <aREST_UI.h>
#include <EEPROM.h>
#include <ArduinoJson.h> // https://arduinojson.org/

EthernetServer server(80);

aREST_UI rest = aREST_UI();

const char* jsonConfig = "{\"net\"{\"ip\":[192,168,1,6],\"mask\":[255,255,255,0],\"gw\":[192,168,1,1],\"dns\":[192,168,1,1],\"mac\":\"001020304050\"},\"mqtt\":\"mqtt.in.tabbo.it\"}";

void saveJsonToEEPROM(const char* json, int startAddr = 0) {
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

void setup() {

  Serial.begin(9600);  
  Serial.println("Start");

  Serial.println("Writing JSON to EEPROM");
  //saveJsonToEEPROM(jsonConfig);
  
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
  }


  if (Ethernet.linkStatus() == LinkOFF) {
    Serial.println("Ethernet cable is not connected.");
  }

  // Set the title
  rest.title("aREST UI Demo");

  // Create button to control pin 5
  rest.button(CONTROLLINO_R1);
  rest.button(CONTROLLINO_R2);
  rest.set_id("1");
  rest.set_name("esp8266");

  server.begin();

  Serial.println("End");


}


void loop() {
  EthernetClient client = server.available();

  if (!client) {
    return;
  }

  while (!client.available()) {
    delay(1);
  }

  rest.handle(client);
}
