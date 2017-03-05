/*
 * target.h
 *
 *  Created on: 23 feb. 2017
 *      Author: michi_000
 */

#ifndef TARGET_H_
#define TARGET_H_

//Harware layout
#define LED_PIN 	D0
#define MOTION_PIN 	D6     // what digital pin we're connected to
#define SETUP_PIN	D1
#define I2C_SDA_PIN	D2
#define SW_PIN 		D3
#define I2C_SCL_PIN	D4


//EEPROM data locations
#define SSID_EPOS	0		//32
#define PWD_EPOS	32		//32
#define OTAS_EPOS	64		//32
#define OTAP_EPOS	96		//8
#define OTAU_EPOS	104		//32
#define MQTS_EPOS	136		//32
#define MQTP_EPOS	168		//8
#define MQTD_EPOS	176		//16
#define MQTL_EPOS	192		//16
#define MQTC_EPOS	208		//32
#define MQTU_EPOS	240		//16
#define MQTW_EPOS	256		//16

#define EEPROM_MAX  300



#endif /* TARGET_H_ */
