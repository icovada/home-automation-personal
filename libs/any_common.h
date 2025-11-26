#include <ArduinoHA.h>

// Returns available RAM in bytes
int freeRam()
{
    extern int __heap_start, *__brkval;
    int v;
    return (int)&v - (__brkval == 0 ? (int)&__heap_start : (int)__brkval);
}

// Initialize MQTT from JSON config
void initMQTT(HADevice &device, HAMqtt &mqtt, StaticJsonDocument<256> &doc, char *mqtt_server, size_t mqtt_server_size, const char *humanName)
{
    device.setName(humanName);
    device.enableSharedAvailability();
    device.enableLastWill();

    if (doc.containsKey("mqtt") && doc["mqtt"].is<const char *>())
    {
        strlcpy(mqtt_server, doc["mqtt"].as<const char *>(), mqtt_server_size);
        mqtt.begin(mqtt_server);
        Serial.println("MQTT initialized");
    }
    else
    {
        Serial.println("WARNING: MQTT config missing, MQTT not initialized");
    }
}