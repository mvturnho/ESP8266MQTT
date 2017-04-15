/*
 * PWMContr.h
 *
 *  Created on: 6 apr. 2017
 *      Author: michi_000
 */

#ifndef PWMCONTR_H_
#define PWMCONTR_H_

#include <Adafruit_PWMServoDriver.h>

#define PWMCHAN 16
#define MAXPWM 4095
#define MAXLEDSSTRIPS 5
#define H2R_MAX_RGB_val 4095.0

class PWMContr {
public:
	struct controldata {
		int pwmindex = 0;
		uint16_t pwm[3];
		uint16_t hsl[3];
		int anim = 0;
		int fade = 0;
		int state = 1;
		int colorcounter;
	};

	PWMContr();
	void initPWM(int activeleds);
	void switchLedStrip(String pwmstr, String payload);
	void pwmLedStrip(String pwmstr, String payload);
	void rgbLedStrip(String pwmstr, String payload);
	void hslLedStrip(String pwmstr, String payload);
    void setAnimate(int stripindex, String payload);
	void dumpPwms(uint16_t *values);
	void animate(void);

private:
	Adafruit_PWMServoDriver pwm = Adafruit_PWMServoDriver();
	int numleds = MAXLEDSSTRIPS;
	controldata control[MAXLEDSSTRIPS];
	int numColors = 1024;


	void setHSL(int index,uint16_t h,uint16_t s, uint16_t l);
	void setPWM(int index,uint16_t r,uint16_t g,uint16_t b);
	void setPWM(int index,uint16_t *colors);
	void HSBtoRGB(int hue, int sat, int bright, uint16_t *colors);

	void writePWM(int index);
	void writePWM(int index,uint16_t *colors);
	void writePWM(int index,uint16_t r,uint16_t g,uint16_t b);

};

#endif /* PWMCONTR_H_ */
