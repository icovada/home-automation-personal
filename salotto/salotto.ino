#define MQTT_NAME "controllino_salotto"
#define MQTT_HUMAN_NAME "Controllino Salotto"

#define DEBUG_MODE 0
#define INPUT_PIN_SIZE 6
#define OUTPUT_PIN_SIZE 9

#include "controllino_common.h"
#include "rest_functions.h"

EthernetServer ethServer(80);
EthernetClient ethMqttClient;
HADevice device("salotto");
HAMqtt mqtt(ethMqttClient, device);
char mqtt_server[32] = "";

PinManager *manager[INPUT_PIN_SIZE];
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
  pins[6] = new RemoteOutputPin(remoteHttpHost, 80, CONTROLLINO_R1); // quadro_esterno_est
  pins[7] = new RemoteOutputPin(remoteHttpHost, 80, CONTROLLINO_R0); // quadro_salotto
  pins[8] = new RemoteOutputPin(remoteHttpHost, 80, CONTROLLINO_R2); // quadro_esterno_ovest

  restoreAndApplyPinStates(pins, outputPinSize);

  // Initialize PinManagers
  manager[0] = new PinManager(CONTROLLINO_A0, false, "studio", pins[0]);
  manager[1] = new PinManager(CONTROLLINO_A1, false, "camera", pins[1]);
  manager[2] = new PinManager(CONTROLLINO_A2, false, "disimpegno", pins[3], pins[5]);
  manager[3] = new PinManager(CONTROLLINO_A3, false, "esterno_est_studio", pins[6], pins[0]);
  manager[4] = new PinManager(CONTROLLINO_A4, false, "esterno_est_salotto", pins[6], pins[7]);
  manager[5] = new PinManager(CONTROLLINO_A5, false, "esterno_ovest_camera", pins[8], pins[1]);

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
