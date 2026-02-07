#define MQTT_NAME "controllino_garage"
#define MQTT_HUMAN_NAME "Controllino Garage"

#define DEBUG_MODE 0
#define INPUT_PIN_SIZE 1
#define OUTPUT_PIN_SIZE 1

#include "controllino_common.h"
#include "rest_functions.h"
#include "basculante.h"

EthernetServer ethServer(80);
EthernetClient ethMqttClient;
HADevice device("garage");
HAMqtt mqtt(ethMqttClient, device);
char mqtt_server[32] = "";

ButtonManager *manager[INPUT_PIN_SIZE];
static Pin *pins[OUTPUT_PIN_SIZE];
static int outputPinSize = OUTPUT_PIN_SIZE;
char remoteHttpHost[64] = "";

BasculanteManager *basculante = nullptr;

void onCoverCommand(HACover::CoverCommand cmd, HACover *sender)
{
  if (!basculante)
    return;

  if (cmd == HACover::CoverCommand::CommandOpen)
  {
    basculante->open();
  }
  else if (cmd == HACover::CoverCommand::CommandClose)
  {
    basculante->close();
  }
  else if (cmd == HACover::CoverCommand::CommandStop)
  {
    basculante->stop();
  }
}

void setup()
{
  wdt_disable();
  initSerial();

  StaticJsonDocument<256> doc;
  initNetworkFromEEPROM(doc, remoteHttpHost, sizeof(remoteHttpHost));

  ethServer.begin();

  // Declare HA devices
  HALight *luce_garage = new HALight("garage_luce");
  luce_garage->setName("Luce");

  HACover *ha_basculante = new HACover("garage_basculante");
  ha_basculante->setName("Basculante");
  ha_basculante->onCommand(onCoverCommand);

  OutputPin::setupGlobalRegistry(pins, &outputPinSize);

  // Output pins — only the light relay is managed as an OutputPin
  pins[0] = new OutputPin(CONTROLLINO_R0, luce_garage);

  restoreAndApplyPinStates(pins, outputPinSize);

  // Basculante: state pin A9, relay up R1, relay down R2
  basculante = new BasculanteManager(CONTROLLINO_A9, CONTROLLINO_R7, CONTROLLINO_R6, ha_basculante, pins[0], CONTROLLINO_A0);

  // Button managers — only the light button
  manager[0] = new ButtonManager(CONTROLLINO_A0, false, "luce_garage", pins[0], nullptr, true);

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

  basculante->check();

  checkPendingStateSave();
  wdt_reset();
}
