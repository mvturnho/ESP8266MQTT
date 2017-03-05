/*
 * PCA9685S.h
 *
 *  Created on: 1 mrt. 2017
 *      Author: michi_000
 */

#ifndef PCA9685S_H_
#define PCA9685S_H_


#include "Arduino.h"

#include "Wire.h"

/**
  Version 1.0.0
	(Semantic Versioning)
**/

//Register defines from data sheet
#define PCA9685_MODE1 0x00 // location for Mode1 register address
#define PCA9685_MODE2 0x01 // location for Mode2 reigster address
#define PCA9685_LED0 0x06 // location for start of LED0 registers

//#define PCA9685_I2C_BASE_ADDRESS B1000000
#define PCA9685_I2C_BASE_ADDRESS    (byte)0x40

class PCA9685
{
  public:
    //NB the i2c address here is the value of the A0, A1, A2, A3, A4 and A5 pins ONLY
    //as the class takes care of its internal base address.
    //so i2cAddress should be between 0 and 63
    PCA9685();
    void begin(int i2cAddress);
    bool init();

	void setLEDOn(int ledNumber);
	void setLEDOff(int ledNumber);
	void setLEDDimmed(int ledNumber, byte amount);
	void writeLED(int ledNumber, word outputStart, word outputEnd);

  private:
	void writeRegister(int regaddress, byte val);
	word readRegister(int regAddress);
	// Our actual i2c address:
	byte _i2cAddress;
};


#endif /* PCA9685S_H_ */
