#include <Arduino.h>
#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <EEPROM.h>
#include <ESP8266httpUpdate.h>

#include <MQTTClient.h>
#include <ArduinoJson.h>
#include <Wire.h>

#include <BME280I2C.h>
#include "BH1750.h"
#include <Adafruit_PWMServoDriver.h>

#include "target.h"

#define SENSOR
#define DEBUG_ESP_HTTP_UPDATE 1

#define SEALEVELPRESSURE_HPA (1013.25)

#define PWMCHAN 16
#define MAXPWM 4095
#define MAXRELAY 2

String version = "TRAP17_SENSOR";
String ssid = "IOT";
String passwd = "nopass";
String otahost = "192.168.3.100";
String otaport = "80";
String otaurl = "/esp/update/ota.php";
String mqtthost = "192.168.3.100";
String mqttport = "1883";
String mqttlocation = "unknown";
String mqttdevice = "default";
String mqttclientid = "";
String mqttuser = "";
String mqttpassword = "";

static const PROGMEM uint8_t delog[] = { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
		1, 1, 1, 1, 1, 1, 1, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 4, 4, 4, 4, 4, 4, 4, 4, 4, 5, 5, 5, 5,
		5, 5, 5, 5, 6, 6, 6, 6, 6, 6, 6, 7, 7, 7, 7, 7, 8, 8, 8, 8, 8, 9, 9, 9, 9, 9, 10, 10, 10, 10, 11, 11, 11, 11, 12, 12, 12, 12, 13, 13, 13, 14,
		14, 14, 15, 15, 15, 16, 16, 16, 17, 17, 18, 18, 18, 19, 19, 20, 20, 21, 21, 22, 22, 23, 23, 24, 24, 25, 25, 26, 26, 27, 28, 28, 29, 30, 30,
		31, 32, 32, 33, 34, 35, 35, 36, 37, 38, 39, 40, 40, 41, 42, 43, 44, 45, 46, 47, 48, 49, 51, 52, 53, 54, 55, 56, 58, 59, 60, 62, 63, 64, 66,
		67, 69, 70, 72, 73, 75, 77, 78, 80, 82, 84, 86, 88, 90, 91, 94, 96, 98, 100, 102, 104, 107, 109, 111, 114, 116, 119, 122, 124, 127, 130, 133,
		136, 139, 142, 145, 148, 151, 155, 158, 161, 165, 169, 172, 176, 180, 184, 188, 192, 196, 201, 205, 210, 214, 219, 224, 229, 234, 239, 244,
		250, };

const char* publisher_json_id = "json";

const char* publisher_switch_id = "switch";
const char* publisher_motion_id = "motion";

const char* subscriber_led_id = "led";
const char* subscriber_rgb_id = "rgb";
const char* subscriber_hsl_id = "hsl";
const char* subscriber_switch_id = "switch";
const char* subscriber_setupmode_id = "setup";

int numleds = 5;

bool metric = true;
unsigned int dopubsw = 0;
unsigned int dopubmotion = 0;

uint16_t pwms[PWMCHAN];
uint16_t hsls[PWMCHAN];

const int relay_pins[] = { RELAY0, RELAY1 };
int relay_values[MAXRELAY];

int lux, oldlux = 0;

WiFiClient net;
MQTTClient client;

//I2C Sensors
BH1750 lightMeter;
BME280I2C bme;
//I2C actuator
//PCA9685 pwmController;                  // Library using default Wire and default linear phase balancing scheme
Adafruit_PWMServoDriver pwm = Adafruit_PWMServoDriver();

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
void readSettingsFromEeprom(void);

void setup() {
	Serial.begin(115200);
	//analogWriteFreq(200);
	Serial.println("Booting " + version);
	EEPROM.begin(512);
	delay(10);
	WiFi.mode(WIFI_STA);

	pinMode(LED_PIN, OUTPUT);
	pinMode(SETUP_PIN, INPUT_PULLUP);
	pinMode(RELAY0, OUTPUT);
	digitalWrite(RELAY0, HIGH);
	pinMode(RELAY1, OUTPUT);
	digitalWrite(RELAY1, HIGH);

#ifdef SENSOR
	pinMode(MOTION_PIN, INPUT);
	pinMode(SW_PIN, INPUT_PULLUP);
	attachInterrupt(SW_PIN, INT_ReleaseSw, FALLING);
	attachInterrupt(MOTION_PIN, INT_Motion, CHANGE);
#endif

	readSettingsFromEeprom();

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

		pwm.begin();
		pwm.setPWMFreq(1600);  // This is the maximum PWM frequency

//		pwmController.resetDevices();       // Software resets all PCA9685 devices on Wire line
//		pwmController.init(B000000);        // Address pins A5-A0 set to B000000
//		pwmController.setPWMFrequency(500); // Default is 200Hz, supports 24Hz to 1526Hz
//		pwmController.setChannelOff(0);
//		pwmController.setChannelPWM(0, 4096);
//
//		//Serial.println(pwmController.getChannelPWM(0)); // Should output 2048, which is 128 << 4
//
		for (int i = 0; i < PWMCHAN; i++) {
			pwms[i] = 0;
//			pwmController.setChannelPWM(i, pwms[i]);
			pwm.setPWM(i, 0, 0);
		}
//
//		pwmController.setChannelsPWM(0, PWMCHAN - 1, pwms);
#endif
		if (ssid.length() > 1) {
			WiFi.begin(ssid.c_str(), passwd.c_str());
		}

		client.begin(mqtthost.c_str(), mqttport.toInt(), net);
		connect();

		t_httpUpdate_return ret = ESPhttpUpdate.update(otahost, otaport.toInt(), otaurl, version);
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

	while (!client.connect(mqttclientid.c_str(), mqttuser.c_str(), mqttpassword.c_str())) {
		Serial.print(".");
		delay(1000);
	}
	Serial.println();
	Serial.println("connected!");
	digitalWrite(LED_PIN, HIGH);
	//client.subscribe(mqttdevice + "." + mqttlocation + "." + subscriber_led_id);
	//client.subscribe(mqttdevice + "." + mqttlocation + "." + subscriber_rgb_id);
	client.subscribe(mqttdevice + "." + mqttlocation + "." + subscriber_switch_id + ".0");
	client.subscribe(mqttdevice + "." + mqttlocation + "." + subscriber_switch_id + ".1");
	client.subscribe(mqttdevice + "." + mqttlocation + "." + subscriber_setupmode_id);
	for (int i = 0; i < (numleds); i++) {
		Serial.println("Subscribe: " + mqttdevice + "." + mqttlocation + "." + subscriber_rgb_id + "." + i);
		client.subscribe(mqttdevice + "." + mqttlocation + "." + subscriber_rgb_id + "." + i);
		//client.subscribe(mqttdevice + "." + mqttlocation + "." + subscriber_hsl_id + "." + i);
		client.subscribe(mqttdevice + "." + mqttlocation + "." + subscriber_rgb_id + "." + i + "." + subscriber_switch_id);
	}

}

void pubswitch(int pin, const char* topic, boolean low_is_on) {
	Serial.print("SW press ");
	Serial.println(topic);
	int value = digitalRead(pin);
	if (value == low_is_on) {
		client.publish(mqttdevice + "." + mqttlocation + "." + topic, "off");
		//digitalWrite(LED_PIN, LOW);
	} else {
		client.publish(mqttdevice + "." + mqttlocation + "." + topic, "on");
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
//		setRGB(rgbstate);
//		Serial.print(".");
	}
}

void sendData(float temp, float hum, float pres, int luxvalue) {
// Reading temperature or humidity takes about 250 milliseconds!
//	hum = dht.readHumidity();
// Read temperature as Celsius (the default)
//temp = dht.readTemperature();

	char t[20];
	dtostrf(temp, 0, 1, t);
	char h[20];
	dtostrf(hum, 0, 1, h);
	char p[20];
	dtostrf(pres, 0, 1, h);

	StaticJsonBuffer<512> jsonBuffer;
	JsonObject& root = jsonBuffer.createObject();
//	root["sensor"] = "BME280";
	root["location"] = mqttlocation;
	JsonObject& data = root.createNestedObject("data");
	data["temp"] = temp;
	data["humidity"] = hum;
	data["pressure"] = pres;
	data["lux"] = luxvalue;

	char buffer[512];
	int size = root.printTo(buffer, sizeof(buffer));

	client.publish(String(mqttdevice + "." + mqttlocation + "." + publisher_json_id).c_str(), buffer, size);

	//root.prettyPrintTo(Serial);
	//Serial.print("\n");
}

void dumpPwms(uint16_t *values) {
	for (int i = 0; i < PWMCHAN; i++) {
		Serial.println("channel: " + String(i) + " - " + values[i]);
	}
}

void getRGB(uint16_t *hsl, uint16_t *pwm, uint16_t index) {
	uint16_t h = hsl[index];
	uint16_t s = hsl[index + 1];
	uint16_t v = hsl[index + 2];

	uint16_t *r = &pwm[index];
	uint16_t *g = &pwm[index + 1];
	uint16_t *b = &pwm[index + 2];

	uint8_t sector = h / 60U;
	uint8_t remainder = (h - sector * 60U) * 64U / 15U;  // 64/15 is really 256/60, but lets stay clear of overflows
	uint8_t p = v * (255U - s) / 255U;  // s and v are (0.0..1.0) --> (0..255), p is (0..255^2)
	uint8_t q = v * (255UL * 255UL - ((long) s) * remainder) / (255UL * 255UL);  // look, fixed point arithmetic in varying encodings
	uint8_t t = v * (255UL * 255UL - ((long) s) * (255U - remainder)) / (255UL * 255UL);

	switch (sector) {
	case 0:
		*r = v;
		*g = t;
		*b = p;
		break;
	case 1:
		*r = q;
		*g = v;
		*b = p;
		break;
	case 2:
		*r = p;
		*g = v;
		*b = t;
		break;
	case 3:
		*r = p;
		*g = q;
		*b = v;
		break;
	case 4:
		*r = t;
		*g = p;
		*b = v;
		break;
	default:            // case 5:
		*r = v;
		*g = p;
		*b = q;
		break;
	}
}

void messageReceived(String topic, String payload, char * bytes, unsigned int length) {
	Serial.print("\nincoming: ");
	Serial.print(topic);
	Serial.print(" - ");
	Serial.print(payload);
	Serial.println();

	if (topic.startsWith(mqttdevice + "." + mqttlocation + "." + subscriber_rgb_id)) {
		int ind = (mqttdevice + "." + mqttlocation + "." + subscriber_rgb_id).length();
		int pwmindex = topic.substring(ind + 1, ind + 2).toInt();
		uint8_t pwmnum = pwmindex * 3;
		Serial.print("PWM rgb-index:" + String(pwmindex) + " channel:" + String(pwmnum));

		//Check for RGB switch
		if (topic.endsWith(subscriber_switch_id)) {
			if (payload.equals("off")) {
				Serial.println(" off");
				pwm.setPWM(pwmnum, 0, 0);
				pwm.setPWM(pwmnum + 1, 0, 0);
				pwm.setPWM(pwmnum + 2, 0, 0);
			} else {
				Serial.println(" on");
				pwm.setPWM(pwmnum, 0, pwms[pwmnum]);
				pwm.setPWM(pwmnum + 1, 0, pwms[pwmnum + 1]);
				pwm.setPWM(pwmnum + 2, 0, pwms[pwmnum + 2]);
			}
		} else if (payload.startsWith("rgb")) {
			Serial.print(" RGB ");
			int r_start = payload.indexOf("(");
			int g_start = payload.indexOf(",", r_start);
			int b_start = payload.indexOf(",", g_start + 1);

			int r = payload.substring(r_start + 1, g_start).toInt();
			int g = payload.substring(g_start + 1, b_start).toInt();
			int b = payload.substring(b_start + 1, payload.length() - 1).toInt();

			Serial.println("R:" + String(r) + " G:" + String(g) + " B:" + String(b));
			if (r > MAXPWM)
				r = MAXPWM;
			if (g > MAXPWM)
				g = MAXPWM;
			if (b > MAXPWM)
				b = MAXPWM;

			pwms[pwmnum] = r;
			pwms[pwmnum + 1] = g;
			pwms[pwmnum + 2] = b;

			pwm.setPWM(pwmnum, 0, pwms[pwmnum]);
			pwm.setPWM(pwmnum + 1, 0, pwms[pwmnum + 1]);
			pwm.setPWM(pwmnum + 2, 0, pwms[pwmnum + 2]);

		} else if (payload.startsWith("hsl")) {
			//int ind = (mqttdevice + "." + mqttlocation + "." + subscriber_rgb_id).length();
			//int pwmindex = topic.substring(ind + 1, ind + 2).toInt();
			//uint8_t pwmnum = pwmindex * 3;
			Serial.println("PWM hsl-index:" + String(pwmindex) + " channel:" + String(pwmnum));

			int h_start = payload.indexOf("(");
			int s_start = payload.indexOf(",", h_start);
			int l_start = payload.indexOf(",", s_start + 1);

			int h = payload.substring(h_start + 1, s_start).toInt();
			int s = payload.substring(s_start + 1, l_start).toInt();
			int l = payload.substring(l_start + 1, payload.length() - 1).toInt();

			hsls[pwmnum] = h;
			hsls[pwmnum + 1] = s;
			hsls[pwmnum + 2] = l;

			Serial.println("HSL:" + String(h) + "," + String(s) + "," + String(l));

			getRGB(hsls, pwms, pwmnum);
			//hsi2rgb(h,s,l,pwms,pwmnum);

			uint16_t r = pgm_read_byte(delog+pwms[pwmnum]) * 16;
			uint16_t g = pgm_read_byte(delog+pwms[pwmnum + 1]) * 16;
			uint16_t b = pgm_read_byte(delog+pwms[pwmnum + 2]) * 16;

			pwm.setPWM(pwmnum, 0, r);
			pwm.setPWM(pwmnum + 1, 0, g);
			pwm.setPWM(pwmnum + 2, 0, b);
			//pgm_read_byte(delog + 3);
		}
	} else if (topic.startsWith(mqttdevice + "." + mqttlocation + "." + subscriber_switch_id)) {
		int index = (mqttdevice + "." + mqttlocation + "." + subscriber_switch_id).length();
		String snum = topic.substring(index + 1);
		int num = snum.toInt();
		Serial.println("index = " + snum);
		if (payload.equals("off")) {
			Serial.println(" off " + num);
			relay_values[num] = HIGH;
			digitalWrite(relay_pins[num], relay_values[num]);
			//digitalWrite(D5, HIGH);
		} else {
			Serial.println(" on " + num);
			relay_values[num] = LOW;
			digitalWrite(relay_pins[num], relay_values[num]);
			//digitalWrite(D5, LOW);
		}
	} else if (topic.equals(mqttdevice + "." + mqttlocation + "." + subscriber_setupmode_id)) {
		setupAP();
		ap_mode = true;
	}
}

String readEeprom(String string, int start, int length) {
	String def = string;
	string = "";
	int end = start + length;
	for (int i = start; i < end; ++i) {
		char c = char(EEPROM.read(i));
		if (c == 0)
			break;
		string += c;
	}
	Serial.println("read: " + string + " default: " + def + " from " + start + " length: " + string.length());
	if (string.length() < 1)
		return def;
	return string;
}

int saveEeprom(String string, int start) {
	int i = 0;
	Serial.println("save " + string + " from " + start);
	if (string.length() > 0) {
		for (i = 0; i < string.length(); ++i) {
			EEPROM.write(i + start, string[i]);
			Serial.print(string[i]);
		}
		Serial.println();
	}
	return i + start;
}

void readSettingsFromEeprom(void) {
	ssid = readEeprom(ssid, SSID_EPOS, 32);
	Serial.println("SSID: " + ssid);

	passwd = readEeprom(passwd, PWD_EPOS, 32);
	Serial.println("PASS: " + passwd);

	otahost = readEeprom(otahost, OTAS_EPOS, 32);
	otaport = readEeprom(otaport, OTAP_EPOS, 8);
	otaurl = readEeprom(otaurl, OTAU_EPOS, 32);
	Serial.println("OTA SERVER: " + otahost + ":" + otaport + otaurl);

	mqtthost = readEeprom(mqtthost, MQTS_EPOS, 32);
	mqttport = readEeprom(mqttport, MQTP_EPOS, 8);
	mqttdevice = readEeprom(mqttdevice, MQTD_EPOS, 16);
	mqttlocation = readEeprom(mqttlocation, MQTL_EPOS, 16);
	mqttclientid = readEeprom(WiFi.macAddress(), MQTC_EPOS, 32);
	mqttuser = readEeprom(mqttuser, MQTU_EPOS, 16);
	mqttpassword = readEeprom(mqttpassword, MQTW_EPOS, 16);
	Serial.println("MQTT SERVER: " + mqtthost + ":" + mqttport + "/" + mqttdevice + "." + mqttlocation);
	Serial.println("MQTT Cliet ID: " + mqttclientid);

	String snumleds = String(numleds);
	snumleds = readEeprom(snumleds, NUMSTRIP, 8);
	numleds = snumleds.toInt();
}

void createWebServer(int webtype) {
	if (webtype == 1) {
		server.on("/",
				[]() {
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
					content += "</td><td><label>password: </label></td><td><input type='text' name='pass' value='"+passwd+"' length=64></td></tr>";
					content += "<tr><td><label>OTA host: </label></td><td><input name='otaserver' value='"+otahost+"' length=32></td><td><label>OTA port: </label></td><td><input name='otaport' value='"+otaport+"' length=8></td></tr>";
					content += "<tr><td><label>OTA url: </label></td><td><input name='otaurl' value='"+otaurl+"' length=32></td></tr>";
					content += "<tr><td><label>MQTT: </label></td><td><input name='mqttserver' value='"+mqtthost+"' length=32></td><td><label>port: </label></td><td><input name='mqttport' value='"+mqttport+"' length=8></td></tr>";
					content += "<tr><td><label>MQTT dev: </label></td><td><input name='mqttdevice' value='"+mqttdevice+"' length=16></td><td><label>MQTT location: </label></td><td><input name='mqttlocation' value='"+mqttlocation+"' length=16></td></tr>";
					content += "<tr><td><label>MQTT clientid: </label></td><td><input name='mqttclid' value='"+mqttclientid+"' length=32></td></tr>";
					content += "<tr><td><label>MQTT user: </label></td><td><input name='mqttuser' value='"+mqttuser+"' length=16></td><td><label>MQTT password: </label></td><td><input name='mqttpwd' value='"+mqttpassword+"' length=16></td></tr>";
					content += "<tr><td><label>#Led Strips: </label></td><td><input name='numleds' value='"+String(numleds)+"' length=8></td></tr>";
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

			if (qsid.length() > 0 && qpass.length() > 0) {
				Serial.println("clearing eeprom");
				for (int i = 0; i < EEPROM_MAX; ++i) {
					EEPROM.write(i, 0);
				}

				saveEeprom(qsid,SSID_EPOS);
				saveEeprom(qpass,PWD_EPOS);
				saveEeprom(qotas,OTAS_EPOS);
				saveEeprom(qotap,OTAP_EPOS);
				saveEeprom(qotau,OTAU_EPOS);
				saveEeprom(qmqtts,MQTS_EPOS);
				saveEeprom(qmqttp,MQTP_EPOS);
				saveEeprom(qmqttd,MQTD_EPOS);
				saveEeprom(qmqttl,MQTL_EPOS);
				saveEeprom(qmqttc,MQTC_EPOS);
				saveEeprom(qmqttu,MQTU_EPOS);
				saveEeprom(qmqttw,MQTW_EPOS);
				saveEeprom(qnl,NUMSTRIP);
				EEPROM.commit();
				delay(500);
				readSettingsFromEeprom();
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
