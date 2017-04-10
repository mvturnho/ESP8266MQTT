#include <Arduino.h>
#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <EEPROM.h>
#include <ESP8266httpUpdate.h>

#include <MQTTClient.h>
#include <ArduinoJson.h>

#include <BME280I2C.h>
#include <Wire.h>
#include "BH1750.h"
#include <Adafruit_PWMServoDriver.h>
#include <I2cDiscreteIoExpander.h>

#include "target.h"
#include "PWMContr.h"
#include "Setup.h"
#include "PCF8574.h"

#define SENSOR
#define DEBUG_ESP_HTTP_UPDATE 1

#define SEALEVELPRESSURE_HPA (1013.25)

#define PWMCHAN 16
#define MAXPWM 4095
#define MAXRELAY 16

String version = "TRAP17_SENSOR";

const char* publisher_json_id = "json";

const char* publisher_switch_id = "switch";
const char* publisher_motion_id = "motion";

const char* subscriber_led_id = "led";
const char* subscriber_rgb_id = "rgb";
const char* subscriber_hsl_id = "hsl";
const char* subscriber_switch_id = "switch";
const char* subscriber_setupmode_id = "setup";

bool haspcf8575 = false;
bool haspcf8574 = false;

bool metric = true;
unsigned int dopubsw = 0;
unsigned int dopubmotion = 0;

uint16_t pwms[PWMCHAN];
uint16_t hsls[PWMCHAN];

static uint16_t ioextender;

int lux, oldlux = 0;

WiFiClient net;
MQTTClient client;

//I2C Sensors
BH1750 lightMeter;
BME280I2C bme;
//I2C actuator
I2cDiscreteIoExpander device;
PWMContr pwmcontr;
Setup iotsetup;

PCF8574 exp74(0x27);  // add leds to lines      (used as output)

//Config variables
boolean ap_mode = false;
const char* apssid = "SETUP_ESP";
const char* passphrase = "setupesp";
ESP8266WebServer server(80);
String content;
String aplist;
int statusCode;

unsigned long lastMillis = 0;
unsigned long luxLastMillis = 0;
int blinkstate = LOW;

#ifdef SENSOR
void INT_ReleaseSw(void);
void INT_Motion(void);
#endif

void sendData(float temp, float hum, float pres, int lux);
void connect(); // <- predefine connect() for setup()
void setupAP(void);

void setup() {
	Serial.begin(115200);

	Serial.println("Booting " + version);
	EEPROM.begin(512);
	delay(10);
	WiFi.mode(WIFI_STA);

	pinMode(LED_PIN, OUTPUT);
	pinMode(SETUP_PIN, INPUT_PULLUP);

#ifdef SENSOR
	pinMode(MOTION_PIN, INPUT);
	pinMode(SW_PIN, INPUT_PULLUP);
	attachInterrupt(SW_PIN, INT_ReleaseSw, FALLING);
	attachInterrupt(MOTION_PIN, INT_Motion, CHANGE);
#endif
	iotsetup.initSetup();

	if (digitalRead(SETUP_PIN) == LOW) {
		setupAP();
		ap_mode = true;
	} else {

#ifdef SENSOR

		Wire.begin(I2C_SDA_PIN, I2C_SCL_PIN);
		Wire.setClock(400000);

		lightMeter.begin();

		if (!bme.begin()) {
			Serial.println("Could not find BME280 sensor!");
		}

		uint8_t status = device.digitalWrite(0x0000);
		if (TWI_SUCCESS != status) {
			Serial.println("Could not find PCF8575!");
		} else {
			haspcf8575 = true;
		}
//
		status = exp74.write8(0);
		if (TWI_SUCCESS != status) {
			Serial.println("Could not find PCF8574!");
		} else {
			haspcf8574 = true;
			Serial.println("found PCF8574!");
		}

		pwmcontr.initPWM(iotsetup.getNumleds());

#endif
		if (iotsetup.getSsid().length() > 1) {
			WiFi.begin(iotsetup.getSsid().c_str(), iotsetup.getPasswd().c_str());
		}

		client.begin(iotsetup.getMqtthost().c_str(), iotsetup.getMqttport().toInt(), net);
		connect();

		t_httpUpdate_return ret = ESPhttpUpdate.update(iotsetup.getOtahost(), iotsetup.getOtaport().toInt(), iotsetup.getOtaurl(), version);
		switch (ret) {
		case HTTP_UPDATE_FAILED:
			Serial.println("[update] Update failed.");
			break;
		case HTTP_UPDATE_NO_UPDATES:
			Serial.println("[update] Update no Update.");
			break;
		case HTTP_UPDATE_OK:
			Serial.println("[update] Update ok."); // may not called we reboot the ESP
			break;
		}
	}
}

//-------------------------------------------
// Interrupt handler
//-------------------------------------------
#ifdef SENSOR
void INT_ReleaseSw(void) {
	dopubsw = 1;
}

void INT_Motion(void) {
	dopubmotion = 1;
}
#endif

String getMacString(void) {
	uint8_t MAC_array[6];
	String macaddress = "";
	WiFi.macAddress(MAC_array);

	for (int i = 0; i < sizeof(MAC_array); ++i) {
		macaddress.concat(String(MAC_array[i], HEX));
	}

	return macaddress;
}

void connect() {
	Serial.println();
	Serial.println("Mac: " + WiFi.macAddress());

	digitalWrite(LED_PIN, LOW);
	Serial.print("checking wifi...");
	while (WiFi.status() != WL_CONNECTED) {
		Serial.print(".");
		delay(500);
	}
	Serial.println();
	Serial.print("connecting...");

	while (!client.connect(iotsetup.getMqttclientid().c_str(), iotsetup.getMqttuser().c_str(), iotsetup.getMqttpassword().c_str())) {
		Serial.print(".");
		delay(1000);
	}
	Serial.println();
	Serial.println("connected!");
	digitalWrite(LED_PIN, HIGH);
	//client.subscribe(mqttdevice + "." + mqttlocation + "." + subscriber_led_id);
	//client.subscribe(mqttdevice + "." + mqttlocation + "." + subscriber_rgb_id);
	if ((haspcf8575 == true) || (haspcf8574 == true)) {
		for (int i = 0; i < iotsetup.getNumoutputs(); i++) {
			Serial.println("Subscribe: " + iotsetup.getMqttdevice() + "." + iotsetup.getMqttlocation() + "." + subscriber_switch_id + "." + i);
			client.subscribe(iotsetup.getMqttdevice() + "." + iotsetup.getMqttlocation() + "." + subscriber_switch_id + "." + i);
		}
		client.subscribe(iotsetup.getMqttdevice() + "." + iotsetup.getMqttlocation() + "." + subscriber_switch_id + ".*");
	}
	client.subscribe(iotsetup.getMqttdevice() + "." + iotsetup.getMqttlocation() + "." + subscriber_setupmode_id);
	for (int i = 0; i < (iotsetup.getNumleds()); i++) {
		Serial.println("Subscribe: " + iotsetup.getMqttdevice() + "." + iotsetup.getMqttlocation() + "." + subscriber_rgb_id + "." + i);
		client.subscribe(iotsetup.getMqttdevice() + "." + iotsetup.getMqttlocation() + "." + subscriber_rgb_id + "." + i);
		//client.subscribe(mqttdevice + "." + mqttlocation + "." + subscriber_hsl_id + "." + i);
		client.subscribe(
				iotsetup.getMqttdevice() + "." + iotsetup.getMqttlocation() + "." + subscriber_rgb_id + "." + i + "." + subscriber_switch_id);
	}
	client.subscribe(iotsetup.getMqttdevice() + "." + iotsetup.getMqttlocation() + "." + subscriber_rgb_id + ".*");
	client.subscribe(iotsetup.getMqttdevice() + "." + iotsetup.getMqttlocation() + "." + subscriber_rgb_id + ".*." + subscriber_switch_id);

}

void pubswitch(int pin, const char* topic, boolean low_is_on) {
	Serial.print("SW press ");
	Serial.println(topic);
	int value = digitalRead(pin);
	if (value == low_is_on) {
		client.publish(iotsetup.getMqttdevice() + "." + iotsetup.getMqttlocation() + "." + topic, "off");
		//digitalWrite(LED_PIN, LOW);
	} else {
		client.publish(iotsetup.getMqttdevice() + "." + iotsetup.getMqttlocation() + "." + topic, "on");
		//digitalWrite(LED_PIN, HIGH);
	}
}

void loop() {
	if (ap_mode) {
		server.handleClient();
		if (millis() - lastMillis > 500) {
			lastMillis = millis();
			if (blinkstate == LOW)
				blinkstate = HIGH;
			else
				blinkstate = LOW;

			digitalWrite(LED_PIN, blinkstate);
		}
	} else {
		client.loop();
		delay(10); // <- fixes some issues with WiFi stability

		if (!client.connected()) {
			connect();
		}

#ifdef SENSOR
		if (client.connected()) { //only send when connected to MQTT server
			if (dopubsw == 1) {
				pubswitch(SW_PIN, publisher_switch_id, true);
				dopubsw = 0;
			} else if (dopubmotion == 1) {
				pubswitch(MOTION_PIN, publisher_motion_id, false);
				dopubmotion = 0;
			}

			if (millis() - lastMillis > 10000) {
				lastMillis = millis();
				float temp(NAN), hum(NAN), pres(NAN);
				uint8_t pressureUnit(4); // unit: B000 = Pa, B001 = hPa, B010 = Hg, B011 = atm, B100 = bar, B101 = torr, B110 = N/m^2, B111 = psi
				bme.read(pres, temp, hum, metric, pressureUnit); // Parameters: (float& pressure, float& temp, float& humidity, bool hPa = true, bool celsius = false)
				lux = lightMeter.readLightLevel();

				sendData(temp, hum, pres, lux);

			}
		}
#endif
	}
}

void sendData(float temp, float hum, float pres, int luxvalue) {
	char t[20];
	dtostrf(temp, 0, 1, t);
	char h[20];
	dtostrf(hum, 0, 1, h);
	char p[20];
	dtostrf(pres, 0, 1, h);

	StaticJsonBuffer<512> jsonBuffer;
	JsonObject& root = jsonBuffer.createObject();
	root["location"] = iotsetup.getMqttlocation();
	JsonObject& data = root.createNestedObject("data");
	data["temp"] = temp;
	data["humidity"] = hum;
	data["pressure"] = pres;
	data["lux"] = luxvalue;

	char buffer[512];
	int size = root.printTo(buffer, sizeof(buffer));

	client.publish(String(iotsetup.getMqttdevice() + "." + iotsetup.getMqttlocation() + "." + publisher_json_id).c_str(), buffer, size);

	//root.prettyPrintTo(Serial);
	//Serial.print("\n");
}

void messageReceived(String topic, String payload, char * bytes, unsigned int length) {
	Serial.print("\nincoming: ");
	Serial.print(topic);
	Serial.print(" - ");
	Serial.print(payload);
	Serial.println();

	if (topic.startsWith(iotsetup.getMqttdevice() + "." + iotsetup.getMqttlocation() + "." + subscriber_rgb_id)) {
		int ind = (iotsetup.getMqttdevice() + "." + iotsetup.getMqttlocation() + "." + subscriber_rgb_id).length();
		String indstr = topic.substring(ind + 1, ind + 2);
		int pwmindex = indstr.toInt();
		uint8_t pwmnum = pwmindex * 3;
		Serial.print("PWM rgb-index:" + String(pwmindex) + " channel:" + String(pwmnum));

		//Check for RGB switch
		if (topic.endsWith(subscriber_switch_id)) {
//			switchLedStrip(indstr, payload);
			pwmcontr.switchLedStrip(indstr, payload);
			//check for rgb values
		} else if (payload.startsWith("pwm")) {
			Serial.print(" PWM ");
//			pwmLedStrip(indstr, payload);
			pwmcontr.pwmLedStrip(indstr, payload);
			//check for hsl values
		} else if (payload.startsWith("rgb")) {
			Serial.print(" RGB ");
			pwmcontr.rgbLedStrip(indstr, payload);
//			rgbLedStrip(indstr, payload);
			//check for hsl values
		} else if (payload.startsWith("hsl")) {
			Serial.println("PWM hsl-index:" + String(pwmindex) + " channel:" + String(pwmnum));
			pwmcontr.hslLedStrip(indstr, payload);
//			hslLedStrip(indstr,payload);
		}
	} else if (topic.startsWith(iotsetup.getMqttdevice() + "." + iotsetup.getMqttlocation() + "." + subscriber_switch_id)) {
		int index = (iotsetup.getMqttdevice() + "." + iotsetup.getMqttlocation() + "." + subscriber_switch_id).length();
		String snum = topic.substring(index + 1);
		uint8_t status;
		if (snum.equals("*")) {
			if (payload.equals("off")) {
				//Serial.println(" off " + num);
				ioextender = 0;
			} else {
				//Serial.println(" on " + num);
				ioextender = 0b1111111111111111;
			}
		} else {
			int num = snum.toInt();

			//Serial.println("index = " + snum);
			if (payload.equals("off")) {
				//Serial.println(" off " + num);
				ioextender &= ~(1 << num);
			} else {
				//Serial.println(" on " + num);
				ioextender |= 1 << num;
			}
		}
		// attempt to write 16-bit word
		if (haspcf8575 == true)
			status = device.digitalWrite(ioextender);
		else if (haspcf8574 == true)
			status = exp74.write8(ioextender);
		if ((haspcf8575 == true) || (haspcf8574 == true))
			if (status != TWI_SUCCESS) {
				Serial.print("write error ");
				Serial.println(status, DEC);
			}

	} else if (topic.equals(iotsetup.getMqttdevice() + "." + iotsetup.getMqttlocation() + "." + subscriber_setupmode_id)) {
		setupAP();
		ap_mode = true;
	}
}

void createWebServer(int webtype) {
	if (webtype == 1) {
		server.on("/", []() {
			IPAddress ip = WiFi.softAPIP();
			String ipStr = String(ip[0]) + '.' + String(ip[1]) + '.' + String(ip[2]) + '.' + String(ip[3]);
			content = "<!DOCTYPE HTML>\r\n<html><h1>Configuration for ESP8266</h1><br/><hr/><p> ";
			content += "<table>";
			content += "<tr><td>version:</td><td>"+version+"</td></tr>";
			content += "<tr><td>Mac:</td><td>"+WiFi.macAddress()+"</td></tr>";
			content += "<tr><td>IP:</td><td>"+ipStr+"</td></tr>";
			content += "</table>";
			content += "<p>";
			content += "</p><form method='get' action='setting'><table>";
			content += "<tr><td><label>SSID: </label></td>";
			content += "<td><input type='text' name='ssid' length=32 list='accesspoints'/><datalist id='accesspoints'>";
			content += aplist;
			content += "</datalist>";
			content += iotsetup.getHTML();
//					content += "</td><td><label>password: </label></td><td><input type='text' name='pass' value='"+passwd+"' length=64></td></tr>";
//					content += "<tr><td><label>OTA host: </label></td><td><input name='otaserver' value='"+otahost+"' length=32></td><td><label>OTA port: </label></td><td><input name='otaport' value='"+otaport+"' length=8></td></tr>";
//					content += "<tr><td><label>OTA url: </label></td><td><input name='otaurl' value='"+otaurl+"' length=32></td></tr>";
//					content += "<tr><td><label>MQTT: </label></td><td><input name='mqttserver' value='"+mqtthost+"' length=32></td><td><label>port: </label></td><td><input name='mqttport' value='"+mqttport+"' length=8></td></tr>";
//					content += "<tr><td><label>MQTT dev: </label></td><td><input name='mqttdevice' value='"+mqttdevice+"' length=16></td><td><label>MQTT location: </label></td><td><input name='mqttlocation' value='"+mqttlocation+"' length=16></td></tr>";
//					content += "<tr><td><label>MQTT clientid: </label></td><td><input name='mqttclid' value='"+mqttclientid+"' length=32></td></tr>";
//					content += "<tr><td><label>MQTT user: </label></td><td><input name='mqttuser' value='"+mqttuser+"' length=16></td><td><label>MQTT password: </label></td><td><input name='mqttpwd' value='"+mqttpassword+"' length=16></td></tr>";
//					content += "<tr><td><label>#Led Strips: </label></td><td><input name='numleds' value='"+String(numleds)+"' length=8></td></tr>";
//					content += "<tr><td><label>#PCF8575: </label></td><td><input name='numoutputs' value='"+String(numoutputs)+"' length=8></td></tr>";
				content += "</table><BR/><input type='submit' value='Save'></form>";
				content += "</html>";
				server.send(200, "text/html", content);
			});
		server.on("/setting", []() {
			String qsid = server.arg("ssid");
			String qpass = server.arg("pass");
			String qotas = server.arg("otaserver");
			String qotap = server.arg("otaport");
			String qotau= server.arg("otaurl");
			String qmqtts = server.arg("mqttserver");
			String qmqttp = server.arg("mqttport");
			String qmqttd = server.arg("mqttdevice");
			String qmqttl = server.arg("mqttlocation");
			String qmqttc = server.arg("mqttclid");
			String qmqttu = server.arg("mqttuser");
			String qmqttw = server.arg("mqttpwd");
			String qnl = server.arg("numleds");
			String qnout = server.arg("numoutputs");

			if (qsid.length() > 0 && qpass.length() > 0) {
				Serial.println("clearing eeprom");
				for (int i = 0; i < EEPROM_MAX; ++i) {
					EEPROM.write(i, 0);
				}

				iotsetup.saveEeprom(qsid,SSID_EPOS);
				iotsetup.saveEeprom(qpass,PWD_EPOS);
				iotsetup.saveEeprom(qotas,OTAS_EPOS);
				iotsetup.saveEeprom(qotap,OTAP_EPOS);
				iotsetup.saveEeprom(qotau,OTAU_EPOS);
				iotsetup.saveEeprom(qmqtts,MQTS_EPOS);
				iotsetup.saveEeprom(qmqttp,MQTP_EPOS);
				iotsetup.saveEeprom(qmqttd,MQTD_EPOS);
				iotsetup.saveEeprom(qmqttl,MQTL_EPOS);
				iotsetup.saveEeprom(qmqttc,MQTC_EPOS);
				iotsetup.saveEeprom(qmqttu,MQTU_EPOS);
				iotsetup.saveEeprom(qmqttw,MQTW_EPOS);
				iotsetup.saveEeprom(qnl,NUMSTRIP);
				iotsetup.saveEeprom(qnout,NUMOUTP);

				EEPROM.commit();
				delay(500);
				iotsetup.readSettingsFromEeprom();
				content = "<!DOCTYPE HTML>\r\n<html>";
				content += "</p><form method='get' action='reboot'>";
				content += "Settings saved succesfully!<P>";
				content += "<input type='submit' value='Reboot'></form></html>";
				statusCode = 200;
			} else {
				content = "{\"Error\":\"404 Select your SSID\"}";
				statusCode = 404;
				Serial.println("Sending 404");
			}
			server.send(statusCode, "text/html", content);
		});
		server.on("/cleareeprom", []() {
			content = "<!DOCTYPE HTML>\r\n<html>";
			content += "<p>Clearing the EEPROM</p></html>";
			server.send(200, "text/html", content);
			Serial.println("clearing eeprom");
			for (int i = 0; i < EEPROM_MAX; ++i) {
				EEPROM.write(i, 0);
			}
			EEPROM.commit();
		});
		server.on("/reboot", []() {
			content = "<!DOCTYPE HTML>\r\n<html>";
			content += "<p>Rebooting ....</p></html>";
			server.send(200, "text/html", content);

			ESP.restart();
		});
	} else if (webtype == 0) {
		server.on("/", []() {
			IPAddress ip = WiFi.localIP();
			String ipStr = String(ip[0]) + '.' + String(ip[1]) + '.' + String(ip[2]) + '.' + String(ip[3]);
			server.send(200, "application/json", "{\"IP\":\"" + ipStr + "\"}");
		});
		server.on("/cleareeprom", []() {
			content = "<!DOCTYPE HTML>\r\n<html>";
			content += "<p>Clearing the EEPROM</p></html>";
			server.send(200, "text/html", content);
			Serial.println("clearing eeprom");
			for (int i = 0; i < EEPROM_MAX; ++i) {
				EEPROM.write(i, 0);
			}
			EEPROM.commit();
		});
	}
}

void launchWeb(int webtype) {
	Serial.println("");
	Serial.println("WiFi connected");
	Serial.print("Local IP: ");
	Serial.println(WiFi.localIP());
	Serial.print("SoftAP IP: ");
	Serial.println(WiFi.softAPIP());
	createWebServer(webtype);
	// Start the server
	server.begin();
	Serial.println("Server started");
}

void setupAP(void) {
	WiFi.mode(WIFI_STA);
	WiFi.disconnect();
	delay(100);
	ESP8266WiFiScanClass wifi;
	int n = wifi.scanNetworks();
	Serial.println("scan done");
	if (n == 0)
		Serial.println("no networks found");
	else {
		Serial.print(n);
		Serial.println(" networks found");
//		for (int i = 0; i < n; ++i) {
//			// Print SSID and RSSI for each network found
//			Serial.print(i + 1);
//			Serial.print(": ");
//			Serial.print(wifi.SSID(i));
//			Serial.print(" (");
//			Serial.print(wifi.RSSI(i));
//			Serial.print(")");
//			Serial.println(
//					(WiFi.encryptionType(i) == ENC_TYPE_NONE) ? " " : "*");
//			delay(10);
//		}
	}
	Serial.println("");
	aplist = "";
	for (int i = 0; i < n; ++i) {
		// Print SSID and RSSI for each network found
		aplist += "<option value='";
		aplist += wifi.SSID(i);
		aplist += "'>";
		aplist += wifi.SSID(i);
		aplist += "(";
		aplist += wifi.RSSI(i);
		aplist += ")";
		aplist += (WiFi.encryptionType(i) == ENC_TYPE_NONE) ? " " : "*";
		aplist += "</option>";
	}
	delay(100);
	//Serial.println(st);
	WiFi.softAP(apssid, passphrase, 0);
	Serial.println("SoftAp initialized.");
	launchWeb(1);
	Serial.println("WebInterface started.");
}
