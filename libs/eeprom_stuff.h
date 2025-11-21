#ifndef EEPROM_STUFF_H
#define EEPROM_STUFF_H

#include <EEPROM.h>

// Forward declarations
void saveStateToEEPROM(byte stateData);
byte restoreStateFromEEPROM();
void checkPendingStateSave();

// State persistence configuration
#define STATE_EEPROM_START 300 // Start address for state storage (away from config)
#define STATE_ROTATION_SIZE 10 // Number of addresses to rotate through (wear leveling)
#define STATE_SAVE_DELAY 5000  // Minimum ms between saves to reduce wear

// Global variables for state management
static unsigned long lastStateSave = 0;
static bool pendingStateSave = false;
static byte pendingStateData = 0;

// Structure for state storage with validation
struct StateStorage
{
    byte stateData; // Bitmap of pin states
    byte checksum;  // XOR checksum
    byte rotation;  // Current rotation index for wear leveling
};

// Save pin states to EEPROM with wear leveling
void saveStateToEEPROM(byte stateData)
{
    unsigned long currentTime = millis();

    // Debounce: don't save if we just saved recently
    if (currentTime - lastStateSave < STATE_SAVE_DELAY)
    {
        pendingStateSave = true;
        pendingStateData = stateData;
        return;
    }

    // Read current rotation index
    byte rotation = EEPROM.read(STATE_EEPROM_START);
    if (rotation >= STATE_ROTATION_SIZE || rotation == 0xFF)
    {
        rotation = 0; // Initialize if corrupted
    }

    // Move to next rotation slot
    rotation = (rotation + 1) % STATE_ROTATION_SIZE;

    // Calculate address for this rotation
    int addr = STATE_EEPROM_START + 1 + (rotation * sizeof(StateStorage));

    // Calculate checksum
    byte checksum = stateData ^ rotation ^ 0xA5; // XOR with magic byte

    // Write state data
    EEPROM.write(addr, stateData);
    EEPROM.write(addr + 1, checksum);
    EEPROM.write(addr + 2, rotation);

    // Update rotation index
    EEPROM.write(STATE_EEPROM_START, rotation);

    lastStateSave = currentTime;
    pendingStateSave = false;

    wdt_reset(); // Reset watchdog during EEPROM operations
}

// Restore pin states from EEPROM
byte restoreStateFromEEPROM()
{
    // Read current rotation index
    byte rotation = EEPROM.read(STATE_EEPROM_START);

    // If uninitialized, return 0 (all lights off)
    if (rotation >= STATE_ROTATION_SIZE || rotation == 0xFF)
    {
        Serial.println("No saved state found, starting with all lights OFF");
        return 0;
    }

    // Calculate address for current rotation
    int addr = STATE_EEPROM_START + 1 + (rotation * sizeof(StateStorage));

    // Read state data
    byte stateData = EEPROM.read(addr);
    byte checksum = EEPROM.read(addr + 1);
    byte storedRotation = EEPROM.read(addr + 2);

    // Validate checksum
    byte expectedChecksum = stateData ^ storedRotation ^ 0xA5;
    if (checksum != expectedChecksum || storedRotation != rotation)
    {
        Serial.println("State checksum failed, starting with all lights OFF");
        return 0;
    }

    Serial.print("Restored state from EEPROM: 0b");
    Serial.println(stateData, BIN);
    return stateData;
}

// Check if pending state needs to be saved (call in loop)
void checkPendingStateSave()
{
    if (pendingStateSave && (millis() - lastStateSave >= STATE_SAVE_DELAY))
    {
        saveStateToEEPROM(pendingStateData);
    }
}

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

#endif // EEPROM_STUFF_H
