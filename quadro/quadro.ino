#define MQTT_NAME "controllino_quadro"
#define MQTT_HUMAN_NAME "Controllino Quadro"

#define DEBUG_MODE 0
#define MANAGER_COUNT 7
#define PIN_COUNT 2

#include "controllino_common.h"
#include "rest_functions.h"
#include "eastron.h"

ModbusClient modbus(Serial3, 1); // RS485 on Serial3, slave ID 1

EthernetServer ethServer(80);
EthernetClient ethMqttClient;
HADevice device("quadro");
HAMqtt mqtt(ethMqttClient, device);
char mqtt_server[16] = "";

PinManager *manager[MANAGER_COUNT];
static Pin *pins[PIN_COUNT];
static int pinCount = 0;
char remoteHttpHost[64] = "";

void setup()
{
  wdt_disable();
  initSerial();

  StaticJsonDocument<256> doc;
  initNetworkFromEEPROM(doc, remoteHttpHost, sizeof(remoteHttpHost));

  ethServer.begin();

  // Declare HALights and HASwitches
  HALight *salotto = new HALight("salotto");
  salotto->setName("Salotto");

  OutputPin::setupGlobalRegistry(pins, &pinCount);

  pins[0] = new OutputPin(CONTROLLINO_R0, salotto);
  pins[1] = new RemoteOutputPin(remoteHttpHost, 80, CONTROLLINO_R2);
  pinCount = 2;

  restoreAndApplyPinStates(pins, 2);

  // Initialize PinManagers
  manager[0] = new PinManager(CONTROLLINO_A0, false, "cucinaled_down");
  manager[1] = new PinManager(CONTROLLINO_A1, false, "cucinaled_up");
  manager[2] = new PinManager(CONTROLLINO_A2, false, "salotto", pins[0]);
  manager[3] = new PinManager(CONTROLLINO_A3, false, "uscita");
  manager[4] = new PinManager(CONTROLLINO_A4, false, "cucina", pins[1]);
  manager[5] = new PinManager(CONTROLLINO_A5, false, "salotto_secondario");
  manager[6] = new PinManager(CONTROLLINO_IN1, false, "campanello");

  Serial.print("Free RAM before: ");
  Serial.println(freeRam());

  configure_eastron_sensors();

  Serial.print("Free RAM after: ");
  Serial.println(freeRam());

  initMQTT(device, mqtt, doc, mqtt_server, sizeof(mqtt_server), MQTT_HUMAN_NAME);

  wdt_enable(WDTO_4S);
  Serial.println("End");
}

// Modbus read state machine
enum ModbusReadState
{
  READ_PHASES,
  READ_TOTAL,
  READ_ENERGY,
  IDLE
};
ModbusReadState modbusState = READ_PHASES;

void loop()
{
  EthernetClient client = ethServer.available();
  handleHttpRequest(client);

  // Reconnect to MQTT if connection lost
  if (!mqtt.isConnected() && mqtt_server[0] != '\0')
  {
    mqtt.begin(mqtt_server);
  }

  if (millis() > modbus.lastScan + 2000)
  {
    modbusState = READ_PHASES;
    modbus.lastScan = millis();
  }

  // Non-blocking Modbus handling
  if (modbus.isIdle())
  {
    switch (modbusState)
    {
    case READ_PHASES:
      modbus.startReadPhasePowers();
      break;
    case READ_TOTAL:
      modbus.startReadTotalPower();
      break;
    case READ_ENERGY:
      modbus.startReadEnergy();
      break;
    case IDLE:
      break;
    }
  }

  mqtt.loop();

  for (int i = 0; i < MANAGER_COUNT; i++)
  {
    manager[i]->check();
  }

  // Check for Modbus response
  int8_t result = modbus.process();
  if (result == 1)
  {
    switch (modbusState)
    {
    case READ_PHASES:
      modbus.getPhasePowers();
      modbusState = READ_TOTAL;
      break;
    case READ_TOTAL:
      modbus.getTotalPower();
      modbusState = READ_ENERGY;
      break;
    case READ_ENERGY:
      modbus.getEnergy();
      modbusState = IDLE;
      break;
    case IDLE:
      break;
    }
  }
  else if (result < 0)
  {
    if (DEBUG_MODE)
    {
      Serial.print("Modbus error in state ");
      Serial.print(modbusState);
      Serial.print(": ");
      switch (result)
      {
      case -1:
        Serial.println("Timeout");
        break;
      case -2:
        Serial.println("Invalid response");
        break;
      case -3:
        Serial.println("CRC error");
        break;
      default:
        Serial.println(result);
        break;
      }
    }
    modbusState = IDLE;
  }

  mqtt.loop();
  for (int i = 0; i < MANAGER_COUNT; i++)
  {
    manager[i]->check();
  }

  checkPendingStateSave();
  wdt_reset();
}
