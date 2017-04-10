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

static const PROGMEM uint8_t delog[] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
	1, 1, 1, 1, 1, 1, 1, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 4, 4, 4, 4, 4, 4, 4, 4, 4, 5, 5, 5, 5,
	5, 5, 5, 5, 6, 6, 6, 6, 6, 6, 6, 7, 7, 7, 7, 7, 8, 8, 8, 8, 8, 9, 9, 9, 9, 9, 10, 10, 10, 10, 11, 11, 11, 11, 12, 12, 12, 12, 13, 13, 13, 14,
	14, 14, 15, 15, 15, 16, 16, 16, 17, 17, 18, 18, 18, 19, 19, 20, 20, 21, 21, 22, 22, 23, 23, 24, 24, 25, 25, 26, 26, 27, 28, 28, 29, 30, 30,
	31, 32, 32, 33, 34, 35, 35, 36, 37, 38, 39, 40, 40, 41, 42, 43, 44, 45, 46, 47, 48, 49, 51, 52, 53, 54, 55, 56, 58, 59, 60, 62, 63, 64, 66,
	67, 69, 70, 72, 73, 75, 77, 78, 80, 82, 84, 86, 88, 90, 91, 94, 96, 98, 100, 102, 104, 107, 109, 111, 114, 116, 119, 122, 124, 127, 130, 133,
	136, 139, 142, 145, 148, 151, 155, 158, 161, 165, 169, 172, 176, 180, 184, 188, 192, 196, 201, 205, 210, 214, 219, 224, 229, 234, 239, 244,
	250,};

class PWMContr {
public:
	PWMContr();
	void initPWM(int activeleds);
	void switchLedStrip(String pwmstr, String payload);
	void pwmLedStrip(String pwmstr, String payload);
	void rgbLedStrip(String pwmstr, String payload);
	void hslLedStrip(String pwmstr, String payload);

	void dumpPwms(uint16_t *values);

private:
	Adafruit_PWMServoDriver pwm = Adafruit_PWMServoDriver();
	uint16_t pwms[PWMCHAN];
	uint16_t hsls[PWMCHAN];
	int numleds = 5;

	void getRGB(uint16_t *hsl, uint16_t *pwm, uint16_t index);
	void setPWM(int pwmnum,int r,int g,int b);

};

#endif /* PWMCONTR_H_ */
