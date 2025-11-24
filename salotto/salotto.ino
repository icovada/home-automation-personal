#define MQTT_NAME "controllino_salotto"
#define MQTT_HUMAN_NAME "Controllino Salotto"

#define DEBUG_MODE 0
#define MANAGER_COUNT 3
#define INPUT_PIN_SIZE 3
#define OUTPUT_PIN_SIZE 6

#include "controllino_common.h"
#include "rest_functions.h"

EthernetServer ethServer(80);
EthernetClient ethMqttClient;
HADevice device("salotto");
HAMqtt mqtt(ethMqttClient, device);
char mqtt_server[32] = "";

PinManager *manager[MANAGER_COUNT];
static Pin *pins[INPUT_PIN_SIZE];
static int outputPinSize = OUTPUT_PIN_SIZE;

void setup()
{
  wdt_disable();
  initSerial();

  StaticJsonDocument<256> doc;
  initNetworkFromEEPROM(doc);

  ethServer.begin();

  // Declare HALights and HASwitches
  HALight *cucina = new HALight("cucina");
  cucina->setName("Cucina");

  HALight *camera = new HALight("camera");
  camera->setName("Camera");

  HALight *sgabuzzino = new HALight("sgabuzzino");
  sgabuzzino->setName("Sgabuzzino");

  HALight *studio = new HALight("studio");
  studio->setName("Studio");

  HALight *disimpegno = new HALight("disimpegno");
  disimpegno->setName("Disimpegno");

  HASwitch *ventola = new HASwitch("ventola");
  ventola->setName("Ventola");

  OutputPin::setupGlobalRegistry(pins, &outputPinSize);

  pins[0] = new OutputPin(CONTROLLINO_R0, studio);
  pins[1] = new OutputPin(CONTROLLINO_R1, camera);
  pins[2] = new OutputPin(CONTROLLINO_R2, cucina);
  pins[3] = new OutputPin(CONTROLLINO_R3, disimpegno);
  pins[4] = new OutputPin(CONTROLLINO_R4, ventola);
  pins[5] = new OutputPin(CONTROLLINO_R6, sgabuzzino);

  restoreAndApplyPinStates(pins, outputPinSize);

  // Initialize PinManagers
  manager[0] = new PinManager(CONTROLLINO_A0, false, "studio", pins[0]);
  manager[1] = new PinManager(CONTROLLINO_A1, false, "camera", pins[1]);
  manager[2] = new PinManager(CONTROLLINO_A2, false, "disimpegno", pins[3], pins[5]);

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

  for (int i = 0; i < MANAGER_COUNT; i++)
  {
    manager[i]->check();
  }

  checkPendingStateSave();
  wdt_reset();
}
