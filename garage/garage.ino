#define MQTT_NAME "controllino_garage"
#define MQTT_HUMAN_NAME "Controllino Garage"

#define DEBUG_MODE 0
#define INPUT_PIN_SIZE 6
#define OUTPUT_PIN_SIZE 9

#include "controllino_common.h"
#include "rest_functions.h"

EthernetServer ethServer(80);
EthernetClient ethMqttClient;
HADevice device("garage");
HAMqtt mqtt(ethMqttClient, device);
char mqtt_server[32] = "";

ButtonManager *manager[INPUT_PIN_SIZE];
static Pin *pins[INPUT_PIN_SIZE];
static int outputPinSize = OUTPUT_PIN_SIZE;
char remoteHttpHost[64] = "";

void setup()
{
  wdt_disable();
  initSerial();

  StaticJsonDocument<256> doc;
  initNetworkFromEEPROM(doc, remoteHttpHost, sizeof(remoteHttpHost));

  ethServer.begin();

  // Declare HALights and HASwitches
  HALight *garage = new HALight("luce_garage");
  garage->setName("Garage");

  HACover *basculante = new HACover("basculante");
  basculante->setName("Basculante Garage");

  OutputPin::setupGlobalRegistry(pins, &outputPinSize);

  pins[0] = new OutputPin(CONTROLLINO_R0, garage);
  pins[1] = new OutputPin(CONTROLLINO_R1, camera);
  pins[2] = new OutputPin(CONTROLLINO_R2, cucina);
  pins[3] = new OutputPin(CONTROLLINO_R3, disimpegno);
  pins[4] = new OutputPin(CONTROLLINO_R4, ventola);
  pins[5] = new OutputPin(CONTROLLINO_R6, sgabuzzino);

  restoreAndApplyPinStates(pins, outputPinSize);

  // Initialize PinManagers
  manager[0] = new ButtonManager(CONTROLLINO_A0, false, "luce_garage", pins[0]);
  manager[1] = new ButtonManager(CONTROLLINO_A1, false, "camera", pins[1]);
  manager[2] = new ButtonManager(CONTROLLINO_A2, false, "disimpegno", pins[3], pins[5]);
  manager[3] = new ButtonManager(CONTROLLINO_A3, false, "esterno_est_studio", pins[6], pins[0]);
  manager[4] = new ButtonManager(CONTROLLINO_A4, false, "esterno_est_salotto", pins[6], pins[7]);
  manager[5] = new ButtonManager(CONTROLLINO_A5, false, "esterno_ovest_camera", pins[8], pins[1]);

  initMQTT(device, mqtt, doc, mqtt_server, sizeof(mqtt_server), MQTT_HUMAN_NAME);

  wdt_enable(WDTO_4S);
  Serial.println("End");
}

void loop()
{
  EthernetClient client = ethServer.available();
  handleHttpRequest(client);

  // Reconnect to MQTT if connection lost
  if (!mqtt.isConnected() && mqtt_server[0] != '\0')
  {
    mqtt.begin(mqtt_server);
  }

  mqtt.loop();

  for (int i = 0; i < INPUT_PIN_SIZE; i++)
  {
    manager[i]->check();
  }

  checkPendingStateSave();
  wdt_reset();
}
