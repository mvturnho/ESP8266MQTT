/*
 * PWMContr.cpp
 *
 *  Created on: 6 apr. 2017
 *      Author: michi_000
 */
#include <Arduino.h>
#include "PWMContr.h"

PWMContr::PWMContr() {
}

void PWMContr::initPWM(int activeleds) {
	pwm.begin();
	pwm.setPWMFreq(1600);  // This is the maximum PWM frequency
	numleds = activeleds;
	for (int i = 0; i < PWMCHAN; i++) {
		pwms[i] = 0;
		pwm.setPWM(i, 0, 0);
	}

}

void PWMContr::switchLedStrip(String pwmstr, String payload) {
	if (pwmstr.equals("*")) {
		if (payload.equals("off"))
			for (int i = 0; i < numleds * 3; i++) {
				pwm.setPWM(i, 0, 0);
			}
		else
			for (int i = 0; i < numleds * 3; i++) {
				pwm.setPWM(i, 0, pwms[i]);
			}
	} else {
		int pwmindex = pwmstr.toInt();
		uint8_t pwmnum = pwmindex * 3;
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
	}
}

void PWMContr::pwmLedStrip(String pwmstr, String payload) {

	int r_start = payload.indexOf("(");
	int g_start = payload.indexOf(",", r_start);
	int b_start = payload.indexOf(",", g_start + 1);

	int r = payload.substring(r_start + 1, g_start).toInt();
	int g = payload.substring(g_start + 1, b_start).toInt();
	int b = payload.substring(b_start + 1, payload.length() - 1).toInt();

	if (pwmstr.equals("*")) {
		for (int i = 0; i < numleds; i += 3) {
			setPWM(i, r, g, b);
		}
	} else {
		int pwmindex = pwmstr.toInt();
		uint8_t pwmnum = pwmindex * 3;
		setPWM(pwmnum, r, g, b);
	}

}

void PWMContr::rgbLedStrip(String pwmstr, String payload) {
	int pwmindex = pwmstr.toInt();
	uint8_t pwmnum = pwmindex * 3;

	int r_start = payload.indexOf("(");
	int g_start = payload.indexOf(",", r_start);
	int b_start = payload.indexOf(",", g_start + 1);

	int r = payload.substring(r_start + 1, g_start).toInt() * 16;
	int g = payload.substring(g_start + 1, b_start).toInt() * 16;
	int b = payload.substring(b_start + 1, payload.length() - 1).toInt() * 16;

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

}

void PWMContr::hslLedStrip(String pwmstr, String payload) {
	int pwmindex = pwmstr.toInt();
	uint8_t pwmnum = pwmindex * 3;

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

	pwms[pwmnum] = pgm_read_byte(delog+pwms[pwmnum]) * 16;
	pwms[pwmnum + 1] = pgm_read_byte(delog+pwms[pwmnum + 1]) * 16;
	pwms[pwmnum + 2] = pgm_read_byte(delog+pwms[pwmnum + 2]) * 16;

	pwm.setPWM(pwmnum, 0, pwms[pwmnum]);
	pwm.setPWM(pwmnum + 1, 0, pwms[pwmnum + 1]);
	pwm.setPWM(pwmnum + 2, 0, pwms[pwmnum + 2]);

}

void PWMContr::setPWM(int pwmnum, int r, int g, int b) {
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
}

void PWMContr::getRGB(uint16_t *hsl, uint16_t *pwm, uint16_t index) {
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

void PWMContr::dumpPwms(uint16_t *values) {
	for (int i = 0; i < PWMCHAN; i++) {
		Serial.println("channel: " + String(i) + " - " + values[i]);
	}
}
