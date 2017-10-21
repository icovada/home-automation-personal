// _________________________________________________________________________________________ //
//|                                                                                         |//
//|                                comPVter-luceVentilatore                                 |//
//|                                        ver. 0.1                                         |//
//|                                                                                         |//
//|                                (C) 2016 Federico Tabbò                                  |//
//|                                                                                         |//
//|   -----------------------------------------------------------------------------------   |//
//|                                                                                         |//
//|          This program is free software; you can redistribute it and/or modify           |//
//|          it under the terms of the GNU General Public License as published by           |//
//|           the Free Software Foundation; either version 3 of the License, or             |//
//|                          (at your option) any later version.                            |//
//|                                                                                         |//
//|             This program is distributed in the hope that it will be useful,             |//
//|             but WITHOUT ANY WARRANTY; without even the implied warranty of              |//
//|              MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the               |//
//|                      GNU General Public License for more details.                       |//
//|                                                                                         |//
//|            You should have received a copy of the GNU General Public License            |//
//|         along with this program; if not, write to the Free Software Foundation,         |//
//|            Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301  USA            |//
//|_________________________________________________________________________________________|//
//                                                                                           //


#include <ESP8266WiFi.h>
#include <PubSubClient.h>
#include <OneWire.h>
#include <DallasTemperature.h>

#define wifi_ssid "ssid"
#define wifi_password "password"

#define mqtt_server "192.168.1.2"
#define mqtt_id "tendeovest"

#define topic_tende_set_nord "roncello/esterno/ovest/set/nord"
#define topic_tende_set_sud "roncello/esterno/ovest/set/sud"

#define topic_tende_status_nord "roncello/esterno/ovest/status/nord"
#define topic_tende_status_sud "roncello/esterno/ovest/status/sud"

#define topic_telecomando_set "roncello/esterno/ovest/set/telecomando"

#define updownseconds 30
#define upcoefficient 1.1

long tNordPosition = 0;
long tSudPosition = 0;

unsigned long tNordActBegin = 0;
unsigned long tSudActBegin = 0;
unsigned long tNordTarget = 0;
unsigned long tSudTarget = 0;
bool tNordGoingUp = false;
bool tSudGoingUp = false;
bool tNordMoving = false;
bool tSudMoving = false;

WiFiClient espClient;
PubSubClient client(espClient);

String sTopic;
String sPayload;

OneWire ds(D5); //pin D4
DallasTemperature DS18B20(&ds);


int setNord(int position){
  Serial.println("SetNord");
  tNordActBegin = millis();
  tNordMoving = true;
  if (position == 100){
    digitalWrite(D2,LOW);
    digitalWrite(D3,HIGH);
    tNordGoingUp = false;
    tNordTarget = millis() + updownseconds*1000 + 7000;
  } else if (position == 0){
    digitalWrite(D2,HIGH);
    digitalWrite(D3,LOW);
    tNordGoingUp = true;
    tNordTarget = millis() + updownseconds*1000 + 7000;
  } else if (position > tNordPosition){
    digitalWrite(D2,LOW);
    digitalWrite(D3,HIGH);
    tNordGoingUp = false;
    //Map position in 100 degrees to total seconds it takes
    tNordTarget = millis() + updownseconds*1000/100*(position-tNordPosition);
  } else if (position < tNordPosition){
    digitalWrite(D2,HIGH);
    digitalWrite(D3,LOW);
    tNordGoingUp = true;
    tNordTarget = millis() + updownseconds*1000/100*(tNordPosition-position);
  }
  Serial.print("Target: ");
  Serial.println(tNordTarget);
}

int setSud(int position){
    Serial.println("SetSud");
    tSudActBegin = millis();
    tSudMoving = true;
  if (position == 100){
    digitalWrite(D0,LOW);
    digitalWrite(D1,HIGH);
    tSudGoingUp = false;
    tSudTarget = millis() + updownseconds*1000 + 7000;
  } else if (position == 0){
    digitalWrite(D0,HIGH);
    digitalWrite(D1,LOW);
    tSudGoingUp = true;
    tSudTarget = millis() + updownseconds*1000 + 7000;
  } else if (position > tNordPosition){
    digitalWrite(D0,LOW);
    digitalWrite(D1,HIGH);
    tSudGoingUp = false;
    //Map position in 100 degrees to total seconds it takes
    tSudTarget = millis() + updownseconds*1000/100*(position-tSudPosition);
  } else if (position < tSudPosition){
    digitalWrite(D0,HIGH);
    digitalWrite(D1,LOW);
    tSudGoingUp = true;
    tSudTarget = millis() + updownseconds*1000/100*(tSudPosition-position);
  }
  Serial.print("Target: ");
  Serial.println(tSudTarget);
}


int stopNord(){
  int newPosition = 0;
  unsigned long temppos;
  Serial.println("STOP NORD");
  Serial.println(tNordGoingUp);
  digitalWrite(D2,LOW);
  digitalWrite(D3,LOW);
  unsigned long tNordTime = 0;
  tNordTime = millis() - tNordActBegin;
  tNordMoving = false;
  temppos = tNordTime/((updownseconds*1000)/100);
  Serial.println(temppos);
  Serial.println(tNordPosition);
  if (tNordGoingUp == true){
    Serial.println("UP");
    newPosition = posValidator(tNordPosition - temppos);
  } else {
    Serial.println("DOWN");
    newPosition = posValidator(tNordPosition + temppos);
  }
    client.publish(topic_tende_status_nord, String(newPosition).c_str());
    return newPosition;
  }


int stopSud(){
  int newPosition = 0;
  unsigned long temppos;
  Serial.println("STOP SUD");
  Serial.println(tSudGoingUp);
  digitalWrite(D0,LOW);
  digitalWrite(D1,LOW);
  unsigned long tSudTime = 0;
  tSudTime = millis() - tSudActBegin;
  tSudMoving = false;
  temppos = tSudTime/((updownseconds*1000)/100);
  Serial.println(temppos);
  Serial.println(tSudPosition);
  if (tSudGoingUp == true){
    Serial.println("UP");
    newPosition = posValidator(tSudPosition - temppos);
  } else {
    Serial.println("DOWN");
    newPosition = posValidator(tSudPosition + temppos);
  }
    client.publish(topic_tende_status_sud, String(newPosition).c_str());
    return newPosition;
  }

int posValidator(long position){
  Serial.println("--------VALIDATOR------------");
  Serial.println(position);
  if (position > 100){
    position = 100;
  }
  if (position < 0){
    position = 0;
  }
    Serial.println("--------VALIDATOR------------");
    Serial.println(position);
    return int(position);
  }

void telecomando(){
  digitalWrite(D8, LOW);
  delay(1500);
  digitalWrite(D8, HIGH);
}


void readTemp(){
  DS18B20.requestTemperatures();
  if (DS18B20.getTempCByIndex(0) != 85){
    client.publish("roncello/esterno/ovest/status/temperatura", String(DS18B20.getTempCByIndex(0)).c_str());
  }
}



//connessione WiFi ---------------------------------------------------------------------------|
void setup_wifi(){
  delay(10);
  Serial.println("///////////////// - WiFi - /////////////////");
  Serial.print("Connessione a ");
  Serial.println(wifi_ssid);
  WiFi.begin(wifi_ssid, wifi_password);
  while(WiFi.status()!=WL_CONNECTED) {
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
void reconnect(){
  while(!client.connected()) {
    Serial.println("///////////////// - MQTT - /////////////////");
    Serial.println("Connessione...");
  
    if(client.connect(mqtt_id)) {
      digitalWrite(D4,HIGH);
      Serial.println("Connesso");
      Serial.println("////////////////////////////////////////////");
  
      client.subscribe("roncello/esterno/ovest/set/#");
      client.subscribe("roncello/timer/1min");
    } else {
      digitalWrite(D4,LOW);
      Serial.print("Connessione fallita, rc=");
      Serial.println(client.state());
      Serial.println("Nuovo tentativo tra 5 secondi");
      delay(5000);
    }
  }
}

//callback MQTT ------------------------------------------------------------------------------|
void callback(char* topic, byte* payload, unsigned int length){
  sTopic=topic;
  sPayload="";

    Serial.print("Messaggio ricevuto [");
    Serial.print(topic);
    Serial.print("] ");
  for(int i=0; i<length; i++) {
    Serial.print((char)payload[i]);
    sPayload+=(char)payload[i];
  }
  Serial.println();


  if (sTopic == topic_tende_set_nord){
  if (sPayload == "STOP") {
    stopNord();
  } else {//if (sPayload.toInt() != tNordPosition) {
    setNord(sPayload.toInt());
  }
}

  if (sTopic == topic_tende_set_sud){
    if (sPayload == "STOP") {
      stopSud();
    } else {//if (sPayload.toInt() != tSudPosition) {
      setSud(sPayload.toInt());
    }
  }
  
  if (sTopic == topic_telecomando_set){
    telecomando();
  }
  
  if (sTopic == "roncello/timer/1min"){
    readTemp();
  }
}


//setup --------------------------------------------------------------------------------------|
void setup(){
  pinMode(D0, OUTPUT);
  pinMode(D1, OUTPUT);
  pinMode(D2, OUTPUT);
  pinMode(D3, OUTPUT);
  pinMode(D4, OUTPUT);
  pinMode(D8, OUTPUT);
  
  digitalWrite(D0, 0);
  digitalWrite(D1, 0);
  digitalWrite(D2, 0);
  digitalWrite(D3, 0);
  digitalWrite(D4, 0);
  digitalWrite(D8, 1);
  
  Serial.begin(115200);
  setup_wifi();
  client.setServer(mqtt_server, 1883);
  client.setCallback(callback);
}

//loop ---------------------------------------------------------------------------------------|
void loop(){
  //controllo connessione mqtt -------------------------------------------------------------|
  if (!client.connected()) {
    reconnect();
  }
  client.loop();

  if (tNordMoving == true){
    if (millis() >= tNordTarget){
      tNordPosition = stopNord();
      Serial.println(tNordPosition);
      client.publish(topic_tende_status_nord, String(tNordPosition).c_str());//, true);
    }
  }

  if (tSudMoving == true){
    if (millis() >= tSudTarget){
      tSudPosition = stopSud();
      client.publish(topic_tende_status_sud, String(tSudPosition).c_str());//, true);
    }
  }

  Serial.println("----------------");
  Serial.println(tNordPosition);
  Serial.println(tNordGoingUp);

  delay(200);
}

