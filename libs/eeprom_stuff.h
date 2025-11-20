#include <EEPROM.h>

void saveJsonToEEPROM(char *json, int startAddr = 0)
{
    int len = strlen(json);

    // Validate length fits in a byte and EEPROM
    if (len > 250 || len == 0)
    {
        Serial.println("ERROR: JSON too large or empty for EEPROM");
        return;
    }

    EEPROM.write(startAddr, (byte)len);

    // Calculate checksum while writing
    byte checksum = 0;
    for (int i = 0; i < len; i++)
    {
        EEPROM.write(startAddr + 1 + i, json[i]);
        checksum ^= json[i];

        // Reset watchdog every 32 bytes to prevent timeout during long writes
        if (i % 32 == 0)
        {
            wdt_reset();
        }
    }

    // Store checksum at end
    EEPROM.write(startAddr + 1 + len, checksum);
    wdt_reset(); // Final reset before completing
    Serial.println("Saved to EEPROM with checksum");
}

String readJsonFromEEPROM(int startAddr = 0)
{
    int len = EEPROM.read(startAddr);

    // Limit max length to prevent stack overflow, check for uninitialized EEPROM
    if (len > 250 || len == 0 || len == 0xFF)
    {
        Serial.println("Invalid EEPROM length");
        return "";
    }

    String result = "";
    result.reserve(len + 1);

    // Read data and calculate checksum
    byte checksum = 0;
    for (int i = 0; i < len; i++)
    {
        char c = (char)EEPROM.read(startAddr + 1 + i);
        result += c;
        checksum ^= c;
    }

    // Verify checksum
    byte storedChecksum = EEPROM.read(startAddr + 1 + len);
    if (checksum != storedChecksum)
    {
        Serial.println("ERROR: EEPROM checksum mismatch - data corrupted");
        return "";
    }

    return result;
}

void parseMacAddress(const char *macStr, byte *macArray)
{
    // Validate MAC string length (should be at least 12 characters for AABBCCDDEEFF)
    if (!macStr || strlen(macStr) < 12)
    {
        Serial.println("ERROR: Invalid MAC address length");
        // Set default MAC on error
        byte defaultMac[] = {0xAE, 0xAD, 0xBE, 0xEF, 0xFE, 0xFF};
        memcpy(macArray, defaultMac, 6);
        return;
    }

    for (int i = 0; i < 6; i++)
    {
        char hex[3] = {macStr[i * 2], macStr[i * 2 + 1], '\0'};
        macArray[i] = (byte)strtol(hex, NULL, 16);
    }
}

IPAddress parseIPAddress(JsonArrayConst address)
{
    return IPAddress(
        address[0].as<int>(),
        address[1].as<int>(),
        address[2].as<int>(),
        address[3].as<int>());
}
