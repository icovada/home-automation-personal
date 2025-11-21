#include <ArduinoJson.h> // https://arduinojson.org/
#include <avr/wdt.h>
#include <Ethernet.h>
#include "pin_manager.h"

#ifndef DEBUG_MODE
#define DEBUG_MODE 0
#endif

#ifndef PIN_COUNT
#define PIN_COUNT 0
#endif

extern Pin *pins[PIN_COUNT];

// Forward declaration of function defined in quadro.ino
void saveJsonToEEPROM(char *json, int startAddr = 0);

// Minimal HTTP request handler - replaces aREST
void handleHttpRequest(EthernetClient &client)
{
  if (!client)
    return;

  char requestLine[256];
  int idx = 0;
  bool lineComplete = false;
  unsigned long timeout = millis() + 100;

  // Read first line of HTTP request
  while (client.connected() && millis() < timeout && !lineComplete)
  {
    if (client.available())
    {
      char c = client.read();
      if (c == '\n')
      {
        lineComplete = true;
      }
      else if (c != '\r' && idx < sizeof(requestLine) - 1)
      {
        requestLine[idx++] = c;
      }
    }
  }
  requestLine[idx] = '\0';

  // Drain remaining request data
  while (client.available())
    client.read();

  if (!lineComplete)
  {
    client.stop();
    return;
  }

  // Parse: "GET /endpoint?params=value HTTP/1.1"
  char *path = strchr(requestLine, ' ');
  if (!path)
  {
    client.stop();
    return;
  }
  path++; // skip space

  char *httpVer = strchr(path, ' ');
  if (httpVer)
    *httpVer = '\0'; // terminate path

  // Extract params after ?params=
  char *params = strstr(path, "?params=");
  if (params)
  {
    *params = '\0'; // terminate path
    params += 8;    // skip "?params="
  }

  // Route requests
  const char *response = "OK";

  if (strcmp(path, "/reset") == 0)
  {
    // Send response before reset
    client.println(F("HTTP/1.1 200 OK"));
    client.println(F("Content-Type: text/plain"));
    client.println(F("Connection: close"));
    client.println();
    client.println(F("Resetting..."));
    client.stop();
    delay(100);
    wdt_enable(WDTO_15MS);
    while (1)
    {
    }
  }
  else if (strcmp(path, "/toggle") == 0 && params)
  {
    int pin = atoi(params);
    bool found = false;
    for (int i = 0; i < PIN_COUNT; i++)
    {
      if (pins[i] && pins[i]->pinNumber == pin)
      {
        pins[i]->toggle();
        found = true;
        break;
      }
    }
    response = found ? "Toggled" : "Pin not found";
  }
  else if (strcmp(path, "/replace_config") == 0 && params)
  {
    // URL decode in place
    char *src = params;
    char *dst = params;
    while (*src)
    {
      if (*src == '+')
      {
        *dst++ = ' ';
        src++;
      }
      else if (*src == '%' && src[1] && src[2])
      {
        char hex[3] = {src[1], src[2], 0};
        *dst++ = (char)strtol(hex, NULL, 16);
        src += 3;
      }
      else
      {
        *dst++ = *src++;
      }
    }
    *dst = '\0';

    StaticJsonDocument<256> doc;
    if (deserializeJson(doc, params))
    {
      response = "Invalid JSON";
    }
    else
    {
      saveJsonToEEPROM(params);
      response = "Config saved";
    }
  }
  else
  {
    response = "Unknown endpoint";
  }

  // Send response
  client.println(F("HTTP/1.1 200 OK"));
  client.println(F("Content-Type: text/plain"));
  client.println(F("Connection: close"));
  client.println();
  client.println(response);
  client.stop();
}