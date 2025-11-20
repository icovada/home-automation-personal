#include <DallasTemperature.h>
#include <ESP8266HTTPUpdateServer.h>
#include <ESP8266WebServer.h>
#include <ESP8266WiFi.h>
#include <OneWire.h>
#include <ArduinoHA.h>
#include <WiFiClient.h>
#include "credentials.h"

OneWire ds(D4);
DallasTemperature sensorManager(&ds);
String mqtt_server = "mqtt.in.tabbo.it"; 

byte sensorlist[10][8] = {
    {0x28, 0xFF, 0xEE, 0x20, 0x60, 0x18, 0x03, 0x9D},
    {0x28, 0xFF, 0xFD, 0x09, 0x60, 0x18, 0x03, 0x4C},
    {0x10, 0x44, 0xBA, 0x59, 0x03, 0x08, 0x00, 0x79},
    {0x10, 0x5E, 0x84, 0x59, 0x03, 0x08, 0x00, 0xCB},
    {0x10, 0x0E, 0xE5, 0x59, 0x03, 0x08, 0x00, 0x39},
    {0x28, 0xFF, 0x7B, 0x0A, 0x60, 0x18, 0x03, 0x93},
    {0x28, 0xFF, 0x93, 0x0D, 0x60, 0x18, 0x03, 0xF1},
    {0x28, 0xFF, 0x41, 0x06, 0x60, 0x18, 0x03, 0xB8},
    {0x28, 0xFF, 0xAA, 0x0D, 0x60, 0x18, 0x03, 0x86},
    {0x10, 0x21, 0xD3, 0x59, 0x03, 0x08, 0x00, 0xB3}};

static HASensorNumber *sensorNumber[10];

WiFiClient mqttClient;
HADevice device("heating_sensor");
HAMqtt mqtt(mqttClient, device);

ESP8266WebServer httpServer(80);
ESP8266HTTPUpdateServer httpUpdater;

unsigned long oldmillis = 0;

void calculateTemp(byte *sensor)
{
  ds.reset();
  ds.select(sensor);
  ds.write(0x44, 0); // start conversion, without parasite power
  mqtt.loop();
  for (int i = 0; i < 10; i++)
  {
    delay(100);
  }
}

float readTemp(byte *sensor)
{
  int i;
  float result;
  byte data[12];
  byte type_s;

  ds.reset();
  ds.select(sensor);
  ds.write(0xBE); // read temperature

  for (i = 0; i < 9; i++)
  { // we need 9 bytes
    data[i] = ds.read();
    Serial.print(data[i]);
  }

  if (OneWire::crc8(data, 8) != data[8])
  {               // Check CRC
    return (250); // If temperature read is wrong, return ludicrously high temp
  }

  //////Code from library example sketch
  int16_t raw = (data[1] << 8) | data[0];
  if (sensor[0] == 0x10)
  {
    raw = raw << 3; // 9 bit resolution default
    if (data[7] == 0x10)
    {
      // "count remain" gives full 12 bit resolution
      raw = (raw & 0xFFF0) + 12 - data[6];
    }
  }
  else
  {
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

void setup()
{
  Serial.begin(115200);

  WiFi.mode(WIFI_STA);
  WiFi.begin(wifi_ssid, wifi_password);
  while (WiFi.status() != WL_CONNECTED)
  {
    delay(500);
    Serial.print(".");
  }

  device.setName("Cippino Riscaldamento");
  device.setUniqueId((const byte *)"cippinoriscaldamento", 9);

  sensorManager.begin();
  for (int i = 0; i < 10; i++)
  {
    sensorManager.setResolution(sensorlist[i], 12);
  }

  sensorNumber[0] = new HASensorNumber("tubo1", HASensorNumber::PrecisionP2);
  sensorNumber[0]->setName("Ritorno 1 Salotto Nord");
  sensorNumber[0]->setUnitOfMeasurement("°C");
  sensorNumber[0]->setIcon("mdi:thermometer");
  sensorNumber[0]->setDeviceClass("temperature");
  sensorNumber[0]->setStateClass("measurement");

  sensorNumber[1] = new HASensorNumber("tubo2", HASensorNumber::PrecisionP2);
  sensorNumber[1]->setName("Ritorno 2 Salotto Sud");
  sensorNumber[1]->setUnitOfMeasurement("°C");
  sensorNumber[1]->setIcon("mdi:thermometer");
  sensorNumber[1]->setDeviceClass("temperature");
  sensorNumber[1]->setStateClass("measurement");

  sensorNumber[2] = new HASensorNumber("tubo3", HASensorNumber::PrecisionP2);
  sensorNumber[2]->setName("Ritorno 3 Cucina");
  sensorNumber[2]->setUnitOfMeasurement("°C");
  sensorNumber[2]->setIcon("mdi:thermometer");
  sensorNumber[2]->setDeviceClass("temperature");
  sensorNumber[2]->setStateClass("measurement");

  sensorNumber[3] = new HASensorNumber("tubo4", HASensorNumber::PrecisionP2);
  sensorNumber[3]->setName("Ritorno 4 Studio Sud");
  sensorNumber[3]->setUnitOfMeasurement("°C");
  sensorNumber[3]->setIcon("mdi:thermometer");
  sensorNumber[3]->setDeviceClass("temperature");
  sensorNumber[3]->setStateClass("measurement");

  sensorNumber[4] = new HASensorNumber("tubo5", HASensorNumber::PrecisionP2);
  sensorNumber[4]->setName("Ritorno 5 Studio Nord");
  sensorNumber[4]->setUnitOfMeasurement("°C");
  sensorNumber[4]->setIcon("mdi:thermometer");
  sensorNumber[4]->setDeviceClass("temperature");
  sensorNumber[4]->setStateClass("measurement");

  sensorNumber[5] = new HASensorNumber("tubo6", HASensorNumber::PrecisionP2);
  sensorNumber[5]->setName("Ritorno 6 Sgabuzzino");
  sensorNumber[5]->setUnitOfMeasurement("°C");
  sensorNumber[5]->setIcon("mdi:thermometer");
  sensorNumber[5]->setDeviceClass("temperature");
  sensorNumber[5]->setStateClass("measurement");

  sensorNumber[6] = new HASensorNumber("tubo7", HASensorNumber::PrecisionP2);
  sensorNumber[6]->setName("Ritorno 7 Camera Est");
  sensorNumber[6]->setUnitOfMeasurement("°C");
  sensorNumber[6]->setIcon("mdi:thermometer");
  sensorNumber[6]->setDeviceClass("temperature");
  sensorNumber[6]->setStateClass("measurement");

  sensorNumber[7] = new HASensorNumber("tubo8", HASensorNumber::PrecisionP2);
  sensorNumber[7]->setName("Ritorno 8 Camera Ovest");
  sensorNumber[7]->setUnitOfMeasurement("°C");
  sensorNumber[7]->setIcon("mdi:thermometer");
  sensorNumber[7]->setDeviceClass("temperature");
  sensorNumber[7]->setStateClass("measurement");

  sensorNumber[8] = new HASensorNumber("tubo9", HASensorNumber::PrecisionP2);
  sensorNumber[8]->setName("Ritorno 9 Bagno");
  sensorNumber[8]->setUnitOfMeasurement("°C");
  sensorNumber[8]->setIcon("mdi:thermometer");
  sensorNumber[8]->setDeviceClass("temperature");
  sensorNumber[8]->setStateClass("measurement");
  
  sensorNumber[9] = new HASensorNumber("tubo0", HASensorNumber::PrecisionP2);
  sensorNumber[9]->setName("Mandata");
  sensorNumber[9]->setUnitOfMeasurement("°C");
  sensorNumber[9]->setIcon("mdi:thermometer");
  sensorNumber[9]->setDeviceClass("temperature");
  sensorNumber[9]->setStateClass("measurement");

  device.enableSharedAvailability();
  device.enableLastWill();

  mqtt.begin(mqtt_server.c_str());

  httpUpdater.setup(&httpServer);
  httpServer.begin();
}

void loop()
{
  httpServer.handleClient();

  // Reconnect to MQTT if connection lost
  if (!mqtt.isConnected())
  {
    mqtt.begin(mqtt_server.c_str());
  }
  mqtt.loop();

  if (millis() / 1000 > oldmillis + 10)
  {
    // This will overflow every 40-ish days
    // but we don't care because it will never
    // be on for more than an hour or two
    Serial.println("Calculate temperature loop");

    for (int i = 0; i < 10; i++)
    {
      calculateTemp(sensorlist[i]);
    }

    for (int i = 0; i < 10; i++)
    {
      float reading;
      reading = readTemp(sensorlist[i]);
      if (reading != 250)
      {
        sensorNumber[i]->setValue(reading);
        delay(100);
      }
    }

    oldmillis = millis() / 1000;
  }
}
