#include <Controllino.h>
#include <SPI.h>
#include <Ethernet.h>
#include <aREST.h>
#include <aREST_UI.h>

byte mac[] = { 0xAE, 0xAD, 0xBE, 0xEF, 0xFE, 0xFF };
EthernetServer server(80);

aREST_UI rest = aREST_UI();

void setup() {

  Serial.begin(9600);  
  Serial.println("Start");
  // start the Ethernet connection and the server:
  Ethernet.begin(mac);

  // Check for Ethernet hardware present
  if (Ethernet.hardwareStatus() == EthernetNoHardware) {
    Serial.println("Ethernet shield was not found.  Sorry, can't run without hardware. :(");
    while (true) {
      delay(1); // do nothing, no point running without Ethernet hardware
    }
  }
  if (Ethernet.linkStatus() == LinkOFF) {
    Serial.println("Ethernet cable is not connected.");
  }

  // Set the title
  rest.title("aREST UI Demo");

  // Create button to control pin 5
  rest.button(CONTROLLINO_R1);
  rest.button(CONTROLLINO_R2);
  rest.set_id("1");
  rest.set_name("esp8266");

  server.begin();

  Serial.println("End");


}

   


void loop() {
  EthernetClient client = server.available();

  if (!client) {
    return;
  }

  while (!client.available()) {
    delay(1);
  }

  rest.handle(client);
}
