/*
This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 3 of the License, or
(at your option) any later version.
This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
GNU General Public License for more details.
You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software Foundation,
Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301  USA
*/

#include <ESP8266HTTPUpdateServer.h>
#include <ESP8266WebServer.h>
#include <ESP8266WiFi.h>
#include <ESP8266mDNS.h>
#include <PubSubClient.h>
#include <WiFiClient.h>

#define wifi_ssid "ssid"
#define wifi_password "password"

#define mqtt_server "192.168.1.2"
#define mqtt_id "ledcucinadimmer"

#define brightness_state_topic "roncello/ledcucina/status/brightnesspwm"
#define brightness_command_topic "roncello/ledcucina/set/brightnesspwm"

#define availability_topic "roncello/ledcucina/status/availability"

WiFiClient espClient;
PubSubClient client(espClient);

ESP8266WebServer httpServer(80);
ESP8266HTTPUpdateServer httpUpdater;

String sTopic;
String sPayload;

// The number of Steps between the output being on and off
const int pwmIntervals = 254;

// The R value in the graph equation
// taken from https://diarmuid.ie/blog/pwm-exponential-led-fading-on-arduino-or-other-platforms
float R;

int brightness = 0;

// connessione WiFi
// ---------------------------------------------------------------------------|
void setup_wifi()
{
	delay(10);
	Serial.println("///////////////// - WiFi - /////////////////");
	Serial.print("Connessione a ");
	Serial.println(wifi_ssid);
	WiFi.mode(WIFI_STA);
	WiFi.begin(wifi_ssid, wifi_password);
	while (WiFi.status() != WL_CONNECTED)
	{
		delay(500);
		Serial.print(".");
	}
	Serial.println("Connesso");
	Serial.print("IP=");
	Serial.println(WiFi.localIP());
	Serial.println("////////////////////////////////////////////");
	Serial.println("");
}

void reconnect()
{
	while (!client.connected())
	{
		Serial.println("Connecting MQTT");
		if (client.connect(mqtt_id))
		{
			client.subscribe("roncello/ledcucina/set/#");
			Serial.println("MQTT connected");
		}
		else
		{
			delay(5000);
		}
	}
}

void callback(char *topic, byte *payload, unsigned int length)
{
	sTopic = topic;
	sPayload = "";

	Serial.print("Messaggio ricevuto [");
	Serial.print(topic);
	Serial.print("] ");
	for (int i = 0; i < length; i++)
	{
		Serial.print((char)payload[i]);
		sPayload += (char)payload[i];
	}
	Serial.println();
	Serial.println(String(sPayload));

	if (sTopic == brightness_command_topic)
	{
		setbrightness(sPayload.toInt());
	}
}

void setbrightness(int target)
{
	if (target > brightness)
	{
		Serial.println("Turning light up");
		int steps = target - brightness;
		int msec = 650 / steps;
		for (int interval = brightness; interval <= target; interval++)
		{
			// Calculate the required PWM value for this interval step
			int pwm = (pow(2, (interval / R)) - 1) * 4;
			// Set the LED output to the calculated brightness
			analogWrite(D1, pwm);
			analogWrite(D2, pwm);
			analogWrite(D3, pwm);
			analogWrite(D4, pwm);
			delay(msec);
		}
	}
	else if (target < brightness)
	{
		Serial.println("Turning light down");
		int steps = brightness - target;
		int msec = 650 / steps;
		for (int interval = brightness; interval >= target; interval--)
		{
			// Calculate the required PWM value for this interval step
			int pwm = (pow(2, (interval / R)) - 1) * 4;
			// Set the LED output to the calculated brightness
			analogWrite(D1, pwm);
			analogWrite(D2, pwm);
			analogWrite(D3, pwm);
			analogWrite(D4, pwm);
			delay(msec);
		}
	}
	brightness = target;
}

void setup()
{
	pinMode(D1, OUTPUT);
	pinMode(D2, OUTPUT);
	pinMode(D3, OUTPUT);
	pinMode(D4, OUTPUT);

	Serial.begin(115200);
	setup_wifi();
	client.setServer(mqtt_server, 1883);
	client.setCallback(callback);

	httpUpdater.setup(&httpServer);
	httpServer.begin();

	R = (pwmIntervals * log10(2)) / (log10(255));
	Serial.println(R);
}

void loop()
{
	if (!client.connected())
	{
		reconnect();
	}
	client.loop();
	httpServer.handleClient();
}
