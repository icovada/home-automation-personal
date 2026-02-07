#ifndef BASCULANTE_H
#define BASCULANTE_H

#include <Arduino.h>
#include <ArduinoHA.h>
#include "button_manager.h"

class BasculanteManager
{
public:
    int pinState;       // input pin to read door state
    int pinUp;          // relay pin to command open
    int pinDown;        // relay pin to command close
    HACover *cover;
    Pin *light;         // light to turn on when door is not closed
    int lightSwitchPin; // light switch pin to read state when door closes

    // State detection
    bool lastPinValue = false;
    unsigned long lastTransitionMillis = 0;
    unsigned long lastCyclePeriod = 0;
    int transitionCount = 0;

    // Relay pulse management
    unsigned long pulseUpStart = 0;
    unsigned long pulseDownStart = 0;
    static const unsigned long PULSE_DURATION = 1000; // 1 second pulse

    // State reporting
    HACover::CoverState lastReportedState = HACover::CoverState::StateStopped;

    BasculanteManager(int pinState, int pinUp, int pinDown, HACover *cover, Pin *light = nullptr, int lightSwitchPin = -1)
        : pinState(pinState), pinUp(pinUp), pinDown(pinDown), cover(cover), light(light), lightSwitchPin(lightSwitchPin)
    {
        pinMode(pinState, INPUT);
        pinMode(pinUp, OUTPUT);
        pinMode(pinDown, OUTPUT);
        digitalWrite(pinUp, LOW);
        digitalWrite(pinDown, LOW);
    }

    static void onCommand(HACover::CoverCommand cmd, HACover *sender)
    {
        // Will be connected via a global pointer set in garage.ino
    }

    void open()
    {
        Serial.println("Basculante: pulse UP");
        digitalWrite(pinUp, HIGH);
        pulseUpStart = millis();
    }

    void close()
    {
        Serial.println("Basculante: pulse DOWN");
        digitalWrite(pinDown, HIGH);
        pulseDownStart = millis();
    }

    void stop()
    {
        Serial.println("Basculante: pulse STOP (both)");
        digitalWrite(pinUp, HIGH);
        digitalWrite(pinDown, HIGH);
        pulseUpStart = millis();
        pulseDownStart = millis();
    }

    void check()
    {
        unsigned long now = millis();

        // Handle relay pulse timeouts
        if (pulseUpStart > 0 && (now - pulseUpStart >= PULSE_DURATION))
        {
            digitalWrite(pinUp, LOW);
            pulseUpStart = 0;
        }
        if (pulseDownStart > 0 && (now - pulseDownStart >= PULSE_DURATION))
        {
            digitalWrite(pinDown, LOW);
            pulseDownStart = 0;
        }

        // Read state pin and detect transitions
        bool currentPin = digitalRead(pinState);

        if (currentPin != lastPinValue)
        {
            // Transition detected
            unsigned long elapsed = now - lastTransitionMillis;
            lastTransitionMillis = now;
            lastPinValue = currentPin;
            transitionCount++;

            // Need at least 2 transitions to measure a half-period
            if (transitionCount >= 2)
            {
                lastCyclePeriod = elapsed;
            }
        }

        // Determine state based on pin behaviour
        unsigned long timeSinceLastTransition = now - lastTransitionMillis;

        HACover::CoverState newState = lastReportedState;

        if (timeSinceLastTransition > 1500)
        {
            // Pin has been stable — determine static state
            transitionCount = 0;
            if (currentPin == HIGH)
            {
                newState = HACover::CoverState::StateClosed;
            }
            else
            {
                newState = HACover::CoverState::StateOpen;
            }
        }
        else if (transitionCount >= 2)
        {
            // Pin is cycling — determine by half-period duration
            // 0.5s cycle = 250ms half-period (range: 100-400ms) → opening
            // 1.0s cycle = 500ms half-period (range: 400-750ms) → closing
            if (lastCyclePeriod >= 100 && lastCyclePeriod < 400)
            {
                newState = HACover::CoverState::StateOpening;
            }
            else if (lastCyclePeriod >= 400 && lastCyclePeriod < 750)
            {
                newState = HACover::CoverState::StateClosing;
            }
        }

        // Report state change to Home Assistant
        if (newState != lastReportedState)
        {
            lastReportedState = newState;
            cover->setState(newState);

            // Turn light on when door is not closed
            if (light)
            {
                if (newState == HACover::CoverState::StateClosed)
                {
                    // Restore light to match the switch state
                    if (lightSwitchPin >= 0 && digitalRead(lightSwitchPin))
                        light->on();
                    else
                        light->off();
                }
                else
                {
                    light->on();
                }
            }

            Serial.print("Basculante state: ");
            switch (newState)
            {
            case HACover::CoverState::StateClosed:
                Serial.println("CLOSED");
                break;
            case HACover::CoverState::StateOpen:
                Serial.println("OPEN");
                break;
            case HACover::CoverState::StateOpening:
                Serial.println("OPENING");
                break;
            case HACover::CoverState::StateClosing:
                Serial.println("CLOSING");
                break;
            default:
                Serial.println("UNKNOWN");
                break;
            }
        }
    }
};

#endif
