// Includes
#include <ESP8266WiFi.h>
#include <DNSServer.h>
#include <ESP8266WebServer.h>
#include <EEPROM.h>

#pragma region WIFI configuration
// User configuration
#define SSID_NAME "Device config"
#define SUBTITLE "A connection to a wifi network is required."
#define TITLE "Networks:"
#define BODY "Create an account to get connected to the internet."
#define POST_TITLE "Validating..."
#define POST_BODY "Network config stored. Device will reboot.</br>Thank you."
#define PASS_TITLE "Credentials"
#define CLEAR_TITLE "Cleared"

// Init System Settings
const byte HTTP_CODE = 200;
const byte DNS_PORT = 53;
const byte TICK_TIMER = 1000;
IPAddress APIP(172, 0, 0, 1); // Gateway

String Credentials = "";
unsigned long bootTime = 0, lastActivity = 0, lastTick = 0, tickCtr = 0;
DNSServer dnsServer;
String networksHtml;
ESP8266WebServer webServer(80);

String input(String argName)
{
	String a = webServer.arg(argName);
	a.replace("<", "&lt;");
	a.replace(">", "&gt;");
	a.substring(0, 200);
	return a;
}

String footer()
{
	return "</div><div class=q><a>&#169; All rights reserved.</a></div>";
}

String header(String t)
{
	String a = String(SSID_NAME);
	String CSS = "article { background: #f2f2f2; padding: 1.3em; }"
							 "body { color: #333; font-family: Century Gothic, sans-serif; font-size: 18px; line-height: 24px; margin: 0; padding: 0; }"
							 "div { padding: 0.5em; }"
							 "h1 { margin: 0.5em 0 0 0; padding: 0.5em; }"
							 "input { width: 100%; padding: 9px 10px; margin: 8px 0; box-sizing: border-box; border-radius: 0; border: 1px solid #555555; }"
							 "label { color: #333; display: block; font-style: italic; font-weight: bold; }"
							 "nav { background: #0066ff; color: #fff; display: block; font-size: 1.3em; padding: 1em; }"
							 "nav b { display: block; font-size: 1.5em; margin-bottom: 0.5em; } "
							 "textarea { width: 100%; }";
	String h = "<!DOCTYPE html><html>"
						 "<head><title>" +
						 a + " :: " + t + "</title>"
															"<meta name=viewport content=\"width=device-width,initial-scale=1\">"
															"<style>" +
						 CSS + "</style></head>"
									 "<body><nav><b>" +
						 a + "</b> " + SUBTITLE + "</nav><div><h1>" + t + "</h1></div><div>";
	return h;
}

String index()
{
	return header(TITLE) + "<div>" + networksHtml + "</ol></div><div><form action=/post method=post>" +
				 "<b>SSID:</b> <center><input type=text name=ssid></input></center>" +
				 "<b>Password:</b> <center><input type=password name=password></input><input type=submit value=\"Sign in\"></form></center>" + footer();
}

String posted()
{
	String ssid = input("ssid");
	String password = input("password");
	Credentials = "<li>Email: <b>" + ssid + "</b></br>Password: <b>" + password + "</b></li>" + Credentials;
	Serial.println("writing eeprom ssid:" + ssid);
	for (int i = 0; i < ssid.length(); ++i)
	{
		EEPROM.write(i, ssid[i]);
	}
	Serial.println("writing eeprom pass:" + password);
	for (int i = 0; i < password.length(); ++i)
	{
		EEPROM.write(32 + i, password[i]);
	}
	EEPROM.commit();

	return header(POST_TITLE) + POST_BODY + footer();
}


void scanNetworks(void)
{
	WiFi.mode(WIFI_STA);
	WiFi.disconnect();
	delay(100);
	int n = WiFi.scanNetworks();

	networksHtml = "<ul>";
	for (int i = 0; i < n; ++i)
	{
		// Print SSID and RSSI for each network found
		networksHtml += "<li>";
		networksHtml += WiFi.SSID(i);
		networksHtml += " <i style=\"font-size:6pt\">(";
		networksHtml += WiFi.RSSI(i);

		networksHtml += ")</i>";
		networksHtml += (WiFi.encryptionType(i) == ENC_TYPE_NONE) ? " " : "*";
		networksHtml += "</li>";
	}
	networksHtml += "</ul>";
	delay(100);
}

bool testWifi(void)
{
	int c = 0;
	Serial.println("Waiting for Wifi to connect (up to 10s)");
	while (c < 20)
	{
		if (WiFi.status() == WL_CONNECTED)
		{
			return true;
		}
		delay(500);
		Serial.print("*");
		c++;
	}
	Serial.println("");
	Serial.println("Connect timed out, opening AP");
	return false;
}

void resetEprom()
{
	for (int i = 0; i < 512; i++)
	{
		EEPROM.write(i, 0);
	}
	EEPROM.end();
}

void startAP()
{
	Serial.println("starting AP");
	bootTime = lastActivity = millis();
	scanNetworks();
	WiFi.mode(WIFI_AP);
	WiFi.softAPConfig(APIP, APIP, IPAddress(255, 255, 255, 0));
	WiFi.softAP(SSID_NAME);
	Serial.println("wating connection...");
	dnsServer.start(DNS_PORT, "*", APIP); // DNS spoofing (Only HTTP)
	webServer.on("/post", []() {
		webServer.send(HTTP_CODE, "text/html", posted());
		BLINK();
		Serial.write("rebooting");
		ESP.reset();
	});
	webServer.onNotFound([]() { lastActivity=millis(); webServer.send(HTTP_CODE, "text/html", index()); });
	webServer.begin();
	digitalWrite(LED_BUILTIN, HIGH);
}

void loadWIFIConf() {
	Serial.println("Reading EEPROM");
	int tries = EEPROM.read(100);
	Serial.println("failed " + String(tries) + " times");
	if (tries >= 3)
	{
		resetEprom();
		ESP.reset();
	}
	String esid;
	int emptyCount = 0;
	for (int i = 0; i < 32; ++i)
	{
		int eprom = EEPROM.read(i);
		if (eprom == 0)
		{
			emptyCount++;
			if (emptyCount > 30)
				return startAP();
		}
		esid += char(eprom);
	}
	String epass = "";
	Serial.println(esid);
	Serial.println(epass);
	for (int i = 32; i < 96; ++i)
	{
		epass += char(EEPROM.read(i));
	}
	EEPROM.write(100, ++tries);
	EEPROM.commit();
	Serial.println("new val " + String(tries) + " times");
	WiFi.begin(esid.c_str(), epass.c_str());
}

#pragma endregion

void BLINK()
{ // The internal LED will blink 5 times when a password is received.
	int count = 0;
	while (count < 3)
	{
		digitalWrite(LED_BUILTIN, LOW);
		delay(250);
		digitalWrite(LED_BUILTIN, HIGH);
		delay(250);
		count = count + 1;
	}
}

void setup()
{
	Serial.begin(115200);
	EEPROM.begin(512);
	pinMode(LED_BUILTIN, OUTPUT);
	loadWIFIConf();
	if (testWifi())
	{
		Serial.println("Succesfully Connected!!!");
		EEPROM.write(100, 0);
		EEPROM.commit();
		while(1){
		// qui fai partire un altro webserver con la logica di quello che vuoi
			BLINK();
		}
		return;
	}
	else
		startAP();
}

void loop()
{
	if ((millis() - lastTick) > TICK_TIMER)
	{
		lastTick = millis();
	}
	dnsServer.processNextRequest();
	webServer.handleClient();
}