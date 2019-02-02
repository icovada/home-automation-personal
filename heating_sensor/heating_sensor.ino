#include <ArduinoJson.h>
#include <DallasTemperature.h>
#include <ESP8266HTTPUpdateServer.h>
#include <ESP8266WebServer.h>
#include <ESP8266WiFi.h>
#include <OneWire.h>
#include <PubSubClient.h>
#include <WiFiClient.h>

#define wifi_ssid "Pinguino Curioso"
#define wifi_password "Martina1/2"

#define mqtt_server "192.168.1.2"
#define mqtt_id "heating_flow_sensor_new"

#define BAUDRATE 115200

OneWire ds(D4);
DallasTemperature sensors(&ds);

byte sensorlist[10][8] = {{0x28, 0xFF, 0xFD, 0x09, 0x60, 0x18, 0x03, 0x4C},
                          {0x10, 0x44, 0xBA, 0x59, 0x03, 0x08, 0x00, 0x79},
                          {0x10, 0x5E, 0x84, 0x59, 0x03, 0x08, 0x00, 0xCB},
                          {0x10, 0x0E, 0xE5, 0x59, 0x03, 0x08, 0x00, 0x39},
                          {0x28, 0xFF, 0x7B, 0x0A, 0x60, 0x18, 0x03, 0x93},
                          {0x28, 0xFF, 0x93, 0x0D, 0x60, 0x18, 0x03, 0xF1},
                          {0x28, 0xFF, 0x41, 0x06, 0x60, 0x18, 0x03, 0xB8},
                          {0x28, 0xFF, 0xAA, 0x0D, 0x60, 0x18, 0x03, 0x86},
                          {0x10, 0x21, 0xD3, 0x59, 0x03, 0x08, 0x00, 0xB3},
                          {0x28, 0xFF, 0xEE, 0x20, 0x60, 0x18, 0x03, 0x9D}};

WiFiClient espClient;
PubSubClient client(espClient);

ESP8266WebServer httpServer(80);
ESP8266HTTPUpdateServer httpUpdater;

unsigned long oldmillis = 0;

// Something to hold the float as we string them

// connessione WiFi
// ---------------------------------------------------------------------------|
void setup_wifi() {
  delay(10);
  Serial.println("///////////////// - WiFi - /////////////////");
  Serial.print("Connessione a ");
  Serial.println(wifi_ssid);
  WiFi.mode(WIFI_STA);
  WiFi.begin(wifi_ssid, wifi_password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("Connesso");
  Serial.print("IP=");
  Serial.println(WiFi.localIP());
  Serial.println("////////////////////////////////////////////");
  Serial.println("");
}

// connessione MQTT
// ---------------------------------------------------------------------------|
void reconnect() {
  unsigned long disconnectMillis = millis();
  while (!client.connected()) {
    Serial.println("///////////////// - MQTT - /////////////////");
    Serial.println("Connessione...");

    if (client.connect(mqtt_id)) {
      // digitalWrite(D2,HIGH);
      Serial.println("Connesso");
      Serial.println("////////////////////////////////////////////");
      client.publish("/riscaldamento/tubi", "READY");
    } else {
      Serial.print("Connessione fallita, rc=");
      Serial.println(client.state());
      Serial.println("Nuovo tentativo tra 5 secondi");
      delay(5000);
    }
  }
}

void calculateTemp(byte *sensor) {
  ds.reset();
  ds.select(sensor);
  ds.write(0x44, 0); // start conversion, without parasite power
  client.loop();
  for (int i = 0; i < 10; i++) {
    delay(100);
  }
}

float readTemp(byte *sensor) {
  int i;
  float result;
  byte data[12];
  byte type_s;

  ds.reset();
  ds.select(sensor);
  ds.write(0xBE); // read temperature

  for (i = 0; i < 9; i++) { // we need 9 bytes
    data[i] = ds.read();
    Serial.print(data[i]);
  }

  if (OneWire::crc8(data, 8) != data[8]) { // Check CRC
    return (250); // If temperature read is wrong, return ludicrously high temp
  }

  //////Code from library example sketch
  int16_t raw = (data[1] << 8) | data[0];
  if (sensor[0] == 0x10) {
    raw = raw << 3; // 9 bit resolution default
    if (data[7] == 0x10) {
      // "count remain" gives full 12 bit resolution
      raw = (raw & 0xFFF0) + 12 - data[6];
    }
  } else {
    byte cfg = (data[4] & 0x60);
    // at lower res, the low bits are undefined, so let's zero them
    if (cfg == 0x00)
      raw = raw & ~7; // 9 bit resolution, 93.75 ms
    else if (cfg == 0x20)
      raw = raw & ~3; // 10 bit res, 187.5 ms
    else if (cfg == 0x40)
      raw = raw & ~1; // 11 bit res, 375 ms
    //// default is 12 bit resolution, 750 ms conversion time
  }
  return ((float)raw / 16.0);
}

void setup() {
  Serial.begin(BAUDRATE);

  setup_wifi();
  client.setServer(mqtt_server, 1883);

  sensors.begin();
  for (int i = 0; i < 10; i++) {
    sensors.setResolution(sensorlist[i], 12);
  }

  httpUpdater.setup(&httpServer);
  httpServer.begin();
}

void loop() {
  if (!client.connected()) {
    reconnect();
  }
  client.loop();
  httpServer.handleClient();

  if (millis() / 1000 > oldmillis + 10) {
    // This will overflow every 40-ish days
    // but we don't care because it will never
    // be on for more than an hour or two
    Serial.println("Calculate temperature loop");

    for (int i = 0; i < 10; i++) {
      calculateTemp(sensorlist[i]);
    }

    for (int i = 0; i < 10; i++) {
      float reading;
      char outstring[15];
      reading = readTemp(sensorlist[i]);
      if (reading != 250) {
        Serial.print("Flow in: ");
        Serial.println(reading);
        dtostrf(reading, 6, 4, outstring);
        String topic = "/riscaldamento/tubi/";
        topic = topic + String(i);
        Serial.println(topic);
        client.publish(String(topic).c_str(), String(outstring).c_str());
        delay(100);
      }
    }

    oldmillis = millis() / 1000;
  }
}
