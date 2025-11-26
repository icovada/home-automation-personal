#include <ArduinoHA.h>
#include <EEPROM.h>

class TendaManager
{
public:
    int pinUp = -1;
    int pinDown = -1;
    int timeUp = -1;
    int timeDown = -1;
    HACover *tenda;
    unsigned long startMillis = 0;
    unsigned long stopMillis = -1;
    unsigned long lastUpdate = 0;
    int startPosition = 0;
    int eepromAddress = -1;

    TendaManager(int pinUp, int pinDown, int timeUp, int timeDown, HACover *tenda, int eepromAddress)
        : pinUp(pinUp),
          pinDown(pinDown),
          timeUp(timeUp),
          timeDown(timeDown),
          tenda(tenda),
          eepromAddress(eepromAddress)
    {
        pinMode(pinUp, OUTPUT);
        pinMode(pinDown, OUTPUT);
        digitalWrite(pinUp, 0);
        digitalWrite(pinDown, 0);
    }

    void loadPosition()
    {
        if (eepromAddress >= 0)
        {
            int savedPosition = EEPROM.read(eepromAddress);
            if (savedPosition <= 100)
            {
                tenda->setPosition(savedPosition);
                Serial.print("Loaded position: ");
                Serial.println(savedPosition);
            }
            else
            {
                // Invalid EEPROM value, set to 0
                tenda->setPosition(100);
                Serial.println("No valid saved position, setting to 0");
            }

            if (savedPosition != 100)
            {
                open();
            }
        }
    }

    void savePosition()
    {
        if (eepromAddress >= 0)
        {
            int currentPosition = tenda->getCurrentPosition();
            EEPROM.write(eepromAddress, currentPosition);
            EEPROM.commit();
            Serial.print("Saved position: ");
            Serial.println(currentPosition);
        }
    }

    void close()
    {
        // go up
        Serial.println("Close");
        stop(true, false);
        tenda->setState(HACover::CoverState::StateClosing);
        startMillis = millis();
        startPosition = tenda->getCurrentPosition();
        digitalWrite(pinUp, true);
    }

    void open()
    {
        // go down
        Serial.println("Open");
        stop(true, false);
        tenda->setState(HACover::CoverState::StateOpening);
        startMillis = millis();
        startPosition = tenda->getCurrentPosition();
        // move one step open before saving so it's always marked open
        tenda->setCurrentPosition(startPosition - 1);
        savePosition();
        digitalWrite(pinDown, true);
    }

    void stop(bool simple = false, bool send = true)
    {
        Serial.println("void stop");
        int curPosition = getPosition();
        if (curPosition == 100)
        {
            tenda->setState(HACover::CoverState::StateOpen);
        }
        if (send)
        {
            if (tenda->getCurrentState() == HACover::CoverState::StateClosing)
            {
                tenda->setState(HACover::CoverState::StateClosed);
            }
            else if (tenda->getCurrentState() == HACover::CoverState::StateOpening)
            {
                if (getPosition() == 100)
                {
                    tenda->setState(HACover::CoverState::StateOpen);
                }
                else
                {
                    tenda->setState(HACover::CoverState::StateStopped);
                }
            }
            lastUpdate = millis();
        }
        tenda->setPosition(curPosition);
        digitalWrite(pinDown, false);
        digitalWrite(pinUp, false);

        if (!simple)
        {
            Serial.println("void stop reset");
            stopMillis = -1;
            sendPosition();
            savePosition();
        }
    }

    void check()
    {
        if (tenda->getCurrentState() == HACover::CoverState::StateOpening || tenda->getCurrentState() == HACover::CoverState::StateClosing)
        {
            if (stopMillis == -1)
            {
                if (millis() - startMillis > 40000)
                {
                    // it had enough
                    Serial.println("Stop end run");
                    stop();
                    return;
                }
            }
            else if (millis() >= stopMillis)
            {
                Serial.println("Stop reached target");
                stop();
            }
            sendPosition();
        }
    }

    void setPosition(int position)
    {
        int percentage_to_go = position - tenda->getCurrentPosition();
        if (percentage_to_go > 0)
        {
            // open, go down
            open();
            stopMillis = millis() + (timeDown / 100 * abs(percentage_to_go));
        }
        else if (percentage_to_go < 0)
        {
            // close, go up
            close();
            stopMillis = millis() + (timeUp / 100 * abs(percentage_to_go));
        }
    }

    int getPosition()
    {
        if (tenda->getCurrentState() == HACover::CoverState::StateOpening)
        {
            // Opening = moving up (retracting) = increasing position toward 100
            int elapsed = (millis() - startMillis) * 100 / (timeUp * 1000);
            return min(startPosition + elapsed, 100);
        }
        else if (tenda->getCurrentState() == HACover::CoverState::StateClosing)
        {
            // Closing = moving down (extending) = decreasing position toward 0
            int elapsed = (millis() - startMillis) * 100 / (timeDown * 1000);
            return max(startPosition - elapsed, 0);
        }
        else
        {
            return tenda->getCurrentPosition();
        }
    }

    void sendPosition()
    {
        int elapsed = -1;
        if (millis() - 1000 > lastUpdate)
        {
            int position = getPosition();
            tenda->setPosition(position);
            lastUpdate = millis();
        }
    }
};
