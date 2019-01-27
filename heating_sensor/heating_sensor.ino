
#include <ESP8266WiFi.h>
#include <PubSubClient.h>
#include <WiFiClient.h>
#include <ESP8266WebServer.h>
#include <ESP8266HTTPUpdateServer.h>
#include <OneWire.h>
#include <DallasTemperature.h>

#define wifi_ssid "ssid"
#define wifi_password "password"

#define mqtt_server "192.168.1.2"
#define mqtt_id "heating_flow_sensor"

#define topic_in "roncello/riscaldamento/flowin"
#define topic_out "roncello/riscaldamento/flowout"

#define BAUDRATE 9600

OneWire ds(D4);
DallasTemperature sensors(&ds);

byte flowInTher[8] = { 
  0x28, 0xE9, 0x2E, 0xC5, 0x6, 0x0, 0x0, 0x92 }; //10
byte flowOutTher[8] = { 
  0x28, 0x23, 0xB0, 0xC4, 0x6, 0x0, 0x0, 0x58 }; //14

WiFiClient espClient;
PubSubClient client(espClient);

ESP8266WebServer httpServer(80);
ESP8266HTTPUpdateServer httpUpdater;

unsigned long oldmillis = 0;

//Something to hold the float as we string them



//connessione WiFi ---------------------------------------------------------------------------|
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

//connessione MQTT ---------------------------------------------------------------------------|
void reconnect() {
  unsigned long disconnectMillis = millis();
  while (!client.connected()) {
    Serial.println("///////////////// - MQTT - /////////////////");
    Serial.println("Connessione...");

    if (client.connect(mqtt_id)) {
      //digitalWrite(D2,HIGH);
      Serial.println("Connesso");
      Serial.println("////////////////////////////////////////////");
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
  ds.write(0x44, 0);        // start conversion, without parasite power
  delay(250);
}

float readTemp(byte *sensor){
  int i;
  float result;
  byte data[12];
  
  ds.reset();
  ds.select(sensor);
  ds.write(0xBE);          // read temperature
  delay(250);
  
  for ( i = 0; i < 9; i++) {           // we need 9 bytes
   data[i] = ds.read();
  }
  
  if (OneWire::crc8(data, 8) != data[8]){   //Check CRC
    return (250); //If temperature read is wrong, return ludicrously high temp
  }
  
  //////Code from library example sketch
  int16_t raw = (data[1] << 8) | data[0];
  return ((float)raw / 16.0);

}

void setup() {
  Serial.begin(BAUDRATE);
  
  //Set sensor resolution
  sensors.begin();
  sensors.setResolution(flowInTher, 12);
  sensors.setResolution(flowOutTher, 12);

  setup_wifi();
  client.setServer(mqtt_server, 1883);

  httpUpdater.setup(&httpServer);
  httpServer.begin();
}

void loop() {
  float flowInRead;
  float flowOutRead;
  char outstring[15];
  int i;
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
    sensors.requestTemperatures();

    for (i = 0; i < 15; i++){
      delay(100);
    }

    char outstring[15];

    flowInRead=readTemp(flowInTher);
    if (flowInRead != 250) {
      Serial.print("Flow in: ");
      Serial.println(flowInRead);
      dtostrf(flowInRead,6,4, outstring);
      client.publish(topic_in, outstring);
    }

    flowOutRead=readTemp(flowOutTher);
    if (flowOutRead != 250) {
      Serial.print("Flow out: ");
      Serial.println(flowOutRead);
      dtostrf(flowOutRead,6,4, outstring);
      client.publish(topic_out, outstring);
    }
    oldmillis = millis()/1000;
    Serial.println("Out of the loop");
  }

}
