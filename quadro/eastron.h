#ifndef MODBUS_H
#define MODBUS_H

#ifndef DEBUG_MODE
#define DEBUG_MODE 0
#endif

#include <ArduinoHA.h> // https://github.com/dawidchyrzynski/arduino-home-assistant/

// Modbus configuration from modbus2.py
// Baudrate: 38400, Parity: N, Stopbits: 2
// Slave ID: 1

extern HAMqtt mqtt;

static HASensorNumber *phase1_watts;
static HASensorNumber *phase2_watts;
static HASensorNumber *phase3_watts;
static HASensorNumber *total_watts;
static HASensorNumber *import_watth;

// Shared strings to avoid RAM duplication
static const char STR_W[] = "W";
static const char STR_WH[] = "Wh";
static const char STR_POWER[] = "power";
static const char STR_ENERGY[] = "energy";
static const char STR_MEASUREMENT[] = "measurement";
static const char STR_TOTAL[] = "total";
static const char STR_ICON_FLASH[] = "mdi:flash";
static const char STR_ICON_METER[] = "mdi:meter-electric";

void configure_eastron_sensors(){
  phase1_watts = new HASensorNumber("phase1_watts", HASensorNumber::PrecisionP0);
  phase2_watts = new HASensorNumber("phase2_watts", HASensorNumber::PrecisionP0);
  phase3_watts = new HASensorNumber("phase3_watts", HASensorNumber::PrecisionP0);
  total_watts = new HASensorNumber("total_watts", HASensorNumber::PrecisionP0);
  import_watth = new HASensorNumber("import_watth", HASensorNumber::PrecisionP0);

  phase1_watts->setName("Fase 1");
  phase1_watts->setUnitOfMeasurement(STR_W);
  phase1_watts->setIcon(STR_ICON_FLASH);
  phase1_watts->setDeviceClass(STR_POWER);
  phase1_watts->setStateClass(STR_MEASUREMENT);

  phase2_watts->setName("Fase 2");
  phase2_watts->setUnitOfMeasurement(STR_W);
  phase2_watts->setIcon(STR_ICON_FLASH);
  phase2_watts->setDeviceClass(STR_POWER);
  phase2_watts->setStateClass(STR_MEASUREMENT);

  phase3_watts->setName("Fase 3");
  phase3_watts->setUnitOfMeasurement(STR_W);
  phase3_watts->setIcon(STR_ICON_FLASH);
  phase3_watts->setDeviceClass(STR_POWER);
  phase3_watts->setStateClass(STR_MEASUREMENT);

  total_watts->setName("Totale");
  total_watts->setUnitOfMeasurement(STR_W);
  total_watts->setIcon(STR_ICON_FLASH);
  total_watts->setDeviceClass(STR_POWER);
  total_watts->setStateClass(STR_MEASUREMENT);

  import_watth->setName("Wh in");
  import_watth->setUnitOfMeasurement(STR_WH);
  import_watth->setIcon(STR_ICON_METER);
  import_watth->setDeviceClass(STR_ENERGY);
  import_watth->setStateClass(STR_TOTAL);
}

// Modbus state machine states
enum ModbusState {
  MODBUS_IDLE,
  MODBUS_WAITING_RESPONSE,
  MODBUS_PROCESSING
};

class ModbusClient {
private:
  Stream* serial;
  uint8_t slaveId;

  // State machine
  ModbusState state;
  uint32_t requestTime;
  uint32_t timeout;
  uint32_t minResponseDelay;  // Minimum delay before checking for response

  // Current request tracking
  uint16_t requestAddress;
  uint16_t requestCount;
  uint8_t expectedResponseSize;

  // Response buffer
  uint8_t responseBuffer[256];
  uint16_t responseIndex;

  // Calculate CRC16 for Modbus RTU
  uint16_t calculateCRC(uint8_t* data, uint8_t length) {
    uint16_t crc = 0xFFFF;
    for (uint8_t i = 0; i < length; i++) {
      crc ^= data[i];
      for (uint8_t j = 0; j < 8; j++) {
        if (crc & 0x0001) {
          crc >>= 1;
          crc ^= 0xA001;
        } else {
          crc >>= 1;
        }
      }
    }
    return crc;
  }

  // Send Modbus request to read input registers
  void sendRequest(uint16_t address, uint16_t count) {
    uint8_t frame[8];
    frame[0] = slaveId;
    frame[1] = 0x04; // Read Input Registers
    frame[2] = (address >> 8) & 0xFF;
    frame[3] = address & 0xFF;
    frame[4] = (count >> 8) & 0xFF;
    frame[5] = count & 0xFF;

    uint16_t crc = calculateCRC(frame, 6);
    frame[6] = crc & 0xFF;
    frame[7] = (crc >> 8) & 0xFF;

    if (DEBUG_MODE) {
      Serial.print("Modbus TX: ");
      for (int i = 0; i < 8; i++) {
        if (frame[i] < 0x10) Serial.print("0");
        Serial.print(frame[i], HEX);
        Serial.print(" ");
      }
      Serial.println();
    }

    // Flush any stale data in RX buffer before sending
    while (serial->available()) {
      serial->read();
    }

    // Enable RS485 transmitter
    Controllino_RS485TxEnable();

    serial->write(frame, 8);
    serial->flush(); // Wait for transmission to complete

    // Switch back to receive mode
    Controllino_RS485RxEnable();

    requestAddress = address;
    requestCount = count;
    expectedResponseSize = 5 + (count * 2); // slave + func + len + data + crc(2)
    responseIndex = 0;
    requestTime = millis();
    state = MODBUS_WAITING_RESPONSE;
  }

  // Read 32-bit float from two consecutive registers (Big Endian)
  float readFloat32(uint16_t reg1, uint16_t reg2) {
    union {
      uint32_t i;
      float f;
    } data;

    data.i = ((uint32_t)reg1 << 16) | reg2;
    return data.f;
  }

public:
  unsigned long lastScan;
  ModbusClient(Stream& serialPort, uint8_t slave = 1)
    : serial(&serialPort), slaveId(slave), state(MODBUS_IDLE), timeout(100), minResponseDelay(15) {}

  // Check if client is idle and ready for new request
  bool isIdle() {
    return state == MODBUS_IDLE;
  }

  // Start reading phase powers (non-blocking)
  bool startReadPhasePowers() {
    if (state != MODBUS_IDLE) return false;
    sendRequest(0x000C, 6);
    return true;
  }

  // Start reading total power (non-blocking)
  bool startReadTotalPower() {
    if (state != MODBUS_IDLE) return false;
    sendRequest(0x0034, 2);
    return true;
  }

  // Start reading energy (non-blocking)
  bool startReadEnergy() {
    if (state != MODBUS_IDLE) return false;
    sendRequest(0x0048, 8);
    return true;
  }

  // Process incoming data (call this frequently in loop)
  // Returns: 0 = still waiting, 1 = success, -1 = timeout, -2 = error, -3 = CRC error
  int8_t process() {
    if (state == MODBUS_IDLE) return 0;

    uint32_t elapsed = millis() - requestTime;

    // Don't check for response until minimum delay has passed
    // This gives the slave device time to process and respond
    if (elapsed < minResponseDelay) {
      return 0; // Still waiting for minimum response time
    }

    // Check timeout
    if (elapsed > timeout) {
      if (DEBUG_MODE) {
        Serial.print("Modbus timeout after ");
        Serial.print(elapsed);
        Serial.println("ms");
      }
      state = MODBUS_IDLE;
      return -1; // Timeout
    }

    // Read available bytes
    while (serial->available() && responseIndex < expectedResponseSize) {
      responseBuffer[responseIndex++] = serial->read();
    }

    if (DEBUG_MODE) {
      // Log received bytes if we got any new data
      static uint16_t lastLoggedIndex = 0;
      if (responseIndex > lastLoggedIndex) {
        Serial.print("Modbus RX (");
        Serial.print(responseIndex);
        Serial.print(" bytes): ");
        for (int i = 0; i < responseIndex; i++) {
          if (responseBuffer[i] < 0x10) Serial.print("0");
          Serial.print(responseBuffer[i], HEX);
          Serial.print(" ");
        }
        Serial.println();
        lastLoggedIndex = responseIndex;
      }
      if (state == MODBUS_IDLE) {
        lastLoggedIndex = 0; // Reset for next request
      }
    }

    // Check if we have complete response
    if (responseIndex >= expectedResponseSize) {
      if (DEBUG_MODE) {
        Serial.print("Modbus response received in ");
        Serial.print(elapsed);
        Serial.print("ms, ");
        Serial.print(responseIndex);
        Serial.println(" bytes");
      }

      // Validate CRC
      uint16_t receivedCRC = responseBuffer[responseIndex - 2] | (responseBuffer[responseIndex - 1] << 8);
      uint16_t calculatedCRC = calculateCRC(responseBuffer, responseIndex - 2);

      if (receivedCRC != calculatedCRC) {
        if (DEBUG_MODE) {
          Serial.print("CRC error: expected 0x");
          Serial.print(calculatedCRC, HEX);
          Serial.print(", got 0x");
          Serial.println(receivedCRC, HEX);
        }
        state = MODBUS_IDLE;
        return -3; // CRC error
      }

      // Check slave ID and function code
      if (responseBuffer[0] != slaveId || responseBuffer[1] != 0x04) {
        if (DEBUG_MODE) {
          Serial.print("Invalid response: slave=");
          Serial.print(responseBuffer[0]);
          Serial.print(", func=");
          Serial.println(responseBuffer[1]);
        }
        state = MODBUS_IDLE;
        return -2; // Invalid response
      }

      state = MODBUS_PROCESSING;
      return 1; // Success
    }

    return 0; // Still waiting
  }

  // Get register value from response (call after process() returns 1)
  uint16_t getRegister(uint8_t index) {
    if (state != MODBUS_PROCESSING) return 0;
    if (index >= requestCount) return 0;

    uint8_t offset = 3 + (index * 2); // Skip slave, func, byte count
    return (responseBuffer[offset] << 8) | responseBuffer[offset + 1];
  }

  // Parse phase powers from response
  bool getPhasePowers() {
    if (state != MODBUS_PROCESSING || requestCount != 6) return false;

    phase1_watts->setValue(readFloat32(getRegister(0), getRegister(1)));
    phase2_watts->setValue(readFloat32(getRegister(2), getRegister(3)));
    phase3_watts->setValue(readFloat32(getRegister(4), getRegister(5)));

    state = MODBUS_IDLE;
    return true;
  }

  // Parse total power from response
  bool getTotalPower() {
    if (state != MODBUS_PROCESSING || requestCount != 2) return false;

    total_watts->setValue(readFloat32(getRegister(0), getRegister(1)));

    state = MODBUS_IDLE;
    return true;
  }

  // Parse energy from response
  bool getEnergy() {
    if (state != MODBUS_PROCESSING || requestCount != 8) return false;

    import_watth->setValue(readFloat32(getRegister(0), getRegister(1)) * 1000);

    state = MODBUS_IDLE;
    return true;
  }

  // Reset state (call if you want to abort current operation)
  void reset() {
    state = MODBUS_IDLE;
    responseIndex = 0;
  }
};

#endif
