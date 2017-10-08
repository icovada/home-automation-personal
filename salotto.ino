#include <Indio.h>
#include <SPI.h>
#include <EthernetIndustruino.h>
#include <PubSubClient.h>
#include <UC1701.h>


//Relè 1 2 3 4
//Letture 5 6
//Pulsanti A1 A2

bool pinUno       = 0;
bool pinDue       = 0;
bool pinTre       = 0;

bool pinA1        = 0;
bool pinA2        = 0;

bool pinCinque    = 0;
bool pinSei       = 0;
bool pinCinqueNew = 0;
bool pinSeiNew    = 0;

int  lcdBrightness = 255;

EthernetClient ethClient;
PubSubClient client;

String sTopic;
String sPayload;

static UC1701 lcd;

#define mqtt_server "192.168.1.2"

#define subscribed_topic "roncello/salotto/set/#"
#define tempOvest "roncello/esterno/ovest/status/temperatura"
#define tempEst "roncello/esterno/est/status/temperatura"

#define topic_salottoEst_set    "roncello/salotto/set/luci/salotto/est"
#define topic_salottoEst_status "roncello/salotto/status/luci/salotto/est"

#define topic_salottoOvest_set    "roncello/salotto/set/luci/salotto/ovest"
#define topic_salottoOvest_status "roncello/salotto/status/luci/salotto/ovest"

#define topic_cucina_soffitto_set    "roncello/salotto/set/luci/cucina/soffitto"
#define topic_cucina_soffitto_status "roncello/salotto/status/luci/cucina/soffitto"

#define topic_campanello_casa "roncello/salotto/status/campanello"

#define topic_lcd_brightness "roncello/salotto/set/industruino/lcdbrightness"

// Enter a MAC address and IP address for your controller below.
byte mac[] = {0xDE, 0xAD, 0xBE, 0xEF, 0xFE, 0xED};
IPAddress ip(192,168,1,3);


bool mqttDo(String payload, String pubTopic, int pinNumber, bool pinStatus){
  pinStatus = !pinStatus;
  if (payload == "ON"){
    Indio.digitalWrite(pinNumber,HIGH);
    client.publish(pubTopic.c_str(), String("ON").c_str(), true);
  } else {
    Indio.digitalWrite(pinNumber,LOW);
    client.publish(pubTopic.c_str(), String("OFF").c_str(), true);
  }
  return pinStatus;
}


void callback(char* topic, byte* payload, unsigned int length){
  sTopic = topic;
  sPayload = "";

  for(int i=0; i<length; i++) {
    Serial.print((char)payload[i]);
    sPayload+=(char)payload[i];
  }
  
  if (sTopic == topic_salottoEst_set){
    pinUno = mqttDo(sPayload,topic_salottoEst_status,1,pinUno);
  }
  if (sTopic == topic_salottoOvest_set){
    pinDue = mqttDo(sPayload,topic_salottoOvest_status,2,pinDue);
  }
  if (sTopic == topic_cucina_soffitto_set){
    pinTre = mqttDo(sPayload,topic_cucina_soffitto_status,3,pinTre);
  }
  if (sTopic == tempEst){
    lcd.setCursor(42,2);
    lcd.print(String(sPayload).c_str());
  }
  if (sTopic == tempOvest){
    lcd.setCursor(42,3);
    lcd.print(String(sPayload).c_str());
  }
  if (sTopic == topic_lcd_brightness){
  	lcdBrightness = map(sPayload.toInt(), 0, 100, 255, 0);
  	analogWrite(13,lcdBrightness);
  }
}

bool physicalToggle(bool old, int pinNumber, String topic){
  old = !old;
  if (old){
    Indio.digitalWrite(pinNumber,HIGH);
    client.publish(topic.c_str(), String("ON").c_str(),true);
    Serial.println("ON");
  } else {
    Indio.digitalWrite(pinNumber,LOW);
    client.publish(topic.c_str(), String("OFF").c_str(),true);
    Serial.println("OFF");
  }
  return old;
 }

bool physicalButton(int pinNumber, bool pinStatus, String topic){
  bool newPinStatus = 0;
  if (pinStatus){
    Indio.digitalWrite(pinNumber,LOW);
    client.publish(topic.c_str(), String("OFF").c_str(),true);
  } else {
    Indio.digitalWrite(pinNumber,HIGH);
    client.publish(topic.c_str(), String("ON").c_str(),true);
    newPinStatus = 1;
  }
  return newPinStatus;
}

void reconnect() {
  // Loop until we're reconnected
  while (!client.connected()) {
    digitalWrite(13,LOW);
    lcd.clear();
    lcd.setCursor(0,0);
    lcd.print("MQTT:");
    lcd.setCursor(30,0);
    lcd.print("Connecting...");
    Serial.print("Attempting MQTT connection...");
    // Attempt to connect
    if (client.connect("industruino")) {
      lcd.setCursor(30,0);
      lcd.print("Connected    ");
      Serial.println("connected");
      client.subscribe(subscribed_topic);
      client.subscribe(tempOvest);
      client.subscribe(tempEst);
      lcd.setCursor(0,2);
      lcd.print("Est:          °c");
      lcd.setCursor(0,3);
      lcd.print("Ovest:        °c");
      analogWrite(13,lcdBrightness);
    } else {
      Serial.print("failed, rc=");
      Serial.print(client.state());
      Serial.println(" try again in 5 seconds");
      lcd.setCursor(30,0);
      lcd.print("FAILED       ");
      lcd.setCursor(0,2);
      lcd.print("                ");
      lcd.setCursor(0,3);
      lcd.print("                ");
      // Wait 5 seconds before retrying
      delay(5000);
    }
  }
}

void setup() {
  lcd.begin();
  lcd.setCursor(0,0);
  lcd.print("Booting...");
  pinMode(10, OUTPUT);     // change this to 53 on a mega
  pinMode(4, OUTPUT);     // change this to 53 on a mega
  pinMode(6, OUTPUT);     // change this to 53 on a mega
  pinMode(13, OUTPUT);
  digitalWrite(10, HIGH);
  digitalWrite(6, HIGH);
  digitalWrite(4, HIGH);
  digitalWrite(13,LOW);
  Serial.begin(9600);
  Serial.println("Vai");

  // Open serial communications and wait for port to open:
  Ethernet.begin(mac, ip);

  delay(5000);
  Serial.println("Ci siamo");
  Indio.digitalMode(1,OUTPUT);  // Salotto ovest
  Indio.digitalMode(2,OUTPUT);  // Salotto est
  Indio.digitalMode(3,OUTPUT);  // Cucina
  Indio.digitalMode(4,OUTPUT);  // Riscaldamento
  Indio.digitalMode(5,INPUT);  // Salotto est
  Indio.digitalMode(6,INPUT);  // Salotto ovest


  Indio.setADCResolution(12);
  Indio.analogReadMode(1, V10_p); // Campanello casa
  Indio.analogReadMode(2, V10_p); // Cucina

  client.setClient(ethClient);
  client.setServer(mqtt_server,1883);
  client.setCallback(callback);

  pinCinque = Indio.digitalRead(5);
  pinSei =    Indio.digitalRead(6);

  Indio.digitalWrite(1,LOW);
  Indio.digitalWrite(2,LOW);
  Indio.digitalWrite(3,LOW);
  Indio.digitalWrite(4,LOW);
}

void loop() {
  if (!client.connected()) {
    reconnect();
  }
  pinCinqueNew = Indio.digitalRead(5);
  pinSeiNew    = Indio.digitalRead(6);

  if (pinCinqueNew != pinCinque){
    pinDue = physicalToggle(pinDue, 2, topic_salottoOvest_status);
  }

  if (pinSeiNew != pinSei){
    pinUno = physicalToggle(pinUno, 1, topic_salottoEst_status);
  }

  if ((Indio.analogRead(1) > 80) && (pinA1 == 0) ){
    Serial.println("Campanello di casa");
    client.publish(topic_campanello_casa, String("ON").c_str());
    pinA1 = 1;
  }
  if ((Indio.analogRead(1) < 10) && (pinA1 == 1)){
    client.publish(topic_campanello_casa, String("OFF").c_str(),true);
    pinA1 = 0;
  }

  if ((Indio.analogRead(2) > 80) && (pinA2 == 0) ){
    Serial.println("Cucina");
    pinTre=physicalToggle(pinTre, 3, topic_cucina_soffitto_status);
    pinA2 = 1;
  }
  if ((Indio.analogRead(2) < 10) && (pinA2 == 1)){
    pinA2 = 0;
  }

  pinCinque = pinCinqueNew;
  pinSei = pinSeiNew;
  client.loop();

  uint8_t j, result;
  uint16_t data[80];


}
