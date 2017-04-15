/*
 * PCF8575.cpp
 *
 *  Created on: 13 apr. 2017
 *      Author: michi_000
 */

#include "PCF8575.h"

// ___________________________________________________ PUBLIC MEMBER FUNCTIONS
/// Constructor.
/// Assigns device address, resets storage object, enables bitwise inversion.
/// \required Call this to construct I2cDiscreteIoExpander object.
/// \par Usage:
/// \code
/// ...
/// I2cDiscreteIoExpander exampleA(1);             // device with address 1
/// I2cDiscreteIoExpander exampleB[2] = { 2, 3 };  // devices with addresses 2, 3
/// I2cDiscreteIoExpander exampleC[2] = { I2cDiscreteIoExpander(4), I2cDiscreteIoExpander(5) }; // alternate constructor syntax; devices with addresses 4, 5
/// I2cDiscreteIoExpander exampleD[8] = { 0, 1, 2, 3, 4, 5, 6, 7 }; // addresses 0..7
/// ...
/// \endcode
PCF8575::PCF8575(uint8_t address) {
	address_ = address & 0b111;
	ports_ = 0;
	shouldInvert_ = true;
}

/// \overload I2cDiscreteIoExpander::I2cDiscreteIoExpander(uint8_t address)
/// Constructor.
/// Assigns device address, resets storage object, enables bitwise inversion.
/// \par Usage:
/// \code
/// ...
/// I2cDiscreteIoExpander device;        // implies device address 0
/// ...
/// \endcode
PCF8575::PCF8575() {
	address_ = 0;
	ports_ = 0;
	shouldInvert_ = true;
}

/// Retrieve discrete values from device.
/// \required Call this from within \c loop() in order to read from device.
/// \retval 0 success
/// \retval 1 length too long for buffer
/// \retval 2 address send, NACK received <b>(device not on bus)</b>
/// \retval 3 data send, NACK received
/// \retval 4 other twi error (lost bus arbitration, bus error, ...)
/// \par Usage:
/// \code
/// ...
/// I2cDiscreteIoExpander device;
/// ...
/// uint8_t status = device.digitalRead();
/// if (TWI_SUCCESS == status)
/// {
///   // do something with device.getPorts()
/// }
/// ...
/// \endcode
uint8_t PCF8575::digitalRead() {
	uint8_t hi, lo, status;

	Wire.beginTransmission(BASE_ADDRESS_ | address_);
	status = Wire.endTransmission();

	if (TWI_SUCCESS == status) {
		if (Wire.requestFrom(BASE_ADDRESS_ | address_, 2) == 2) {
			lo = Wire.read();
			hi = Wire.read();
			ports_ = shouldInvert_ ? word(~hi, ~lo) : word(hi, lo);
		} else {
			return TWI_ERROR;
		}
	}

	return status;
}

/// Write discrete values to device.
/// \required Call this from within \c loop() in order to write to device.
/// \param ports word to be written to device (0x0000..0xFFFF)
/// \retval 0 success
/// \retval 1 length too long for buffer
/// \retval 2 address send, NACK received <b>(device not on bus)</b>
/// \retval 3 data send, NACK received
/// \retval 4 other twi error (lost bus arbitration, bus error, ...)
/// \par Usage:
/// \code
/// ...
/// I2cDiscreteIoExpander device;
/// ...
/// uint8_t status = device.digitalWrite(0xFFFF);
/// if (TWI_SUCCESS == status)
/// {
///   // do something
/// }
/// ...
/// \endcode
uint8_t PCF8575::digitalWrite(uint16_t ports) {
	Serial.println("PCF8575::digitalWrite" + String(ports));
	ports_ = shouldInvert_ ? ~ports : ports;
	Wire.beginTransmission(BASE_ADDRESS_ | address_);
	Wire.write(lowByte(ports_));
	Wire.write(highByte(ports_));
	// Wire.write(lowByte(shouldInvert_ ? ~ports_ : ports_));
	// Wire.write(highByte(shouldInvert_ ? ~ports_ : ports_));

	return Wire.endTransmission();
}

/// Retrieve device address.
/// \optional This function is for testing and troubleshooting.
/// \return address of device (0..7)
/// \par Usage:
/// \code
/// ...
/// I2cDiscreteIoExpander device;
/// ...
/// address = device.getAddress();
/// ...
/// \endcode
uint8_t PCF8575::getAddress() {
	return address_;
}

/// Retrieve ports 1 (P17..P10), 0 (P07..P00).
/// \required Call this from within \c loop() to retrieve ports.
/// \return ports of device (0x0000..0xFFFF)
/// \par Usage:
/// \code
/// ...
/// I2cDiscreteIoExpander device;
/// ...
/// ports = device.getPorts();
/// ...
/// \endcode
uint16_t PCF8575::getPorts() {
	return ports_;
}

/// Enable bitwise inversion.
/// All bits will be inverted prior to future read/write operations.
/// \par Usage:
/// \code
/// ...
/// I2cDiscreteIoExpander device;
/// ...
/// device.enableBitwiseInversion();      // bits will now inverted
/// ...
/// \endcode
/// \sa I2cDiscreteIoExpander::disableBitwiseInversion()
/// \sa I2cDiscreteIoExpander::isInverted()
void PCF8575::enableBitwiseInversion() {
	ports_ = shouldInvert_ ? ports_ : ~ports_;
	shouldInvert_ = true;
}

/// Disable bitwise inversion.
/// Bits will not be inverted prior to future read/write operations.
/// \par Usage:
/// \code
/// ...
/// I2cDiscreteIoExpander device;
/// ...
/// device.disableBitwiseInversion();     // bits will no longer be inverted
/// ...
/// \endcode
/// \sa I2cDiscreteIoExpander::enableBitwiseInversion()
/// \sa I2cDiscreteIoExpander::isInverted()
void PCF8575::disableBitwiseInversion() {
	ports_ = shouldInvert_ ? ~ports_ : ports_;
	shouldInvert_ = false;
}

/// Indicate whether bitwise inversion is enabled.
/// \return status of bitwise inversion (false=not inverted, true=inverted)
/// \par Usage:
/// \code
/// ...
/// I2cDiscreteIoExpander device;
/// ...
/// if (device.isInverted())
/// {
///   // do something
/// }
/// ...
/// \endcode
/// \sa I2cDiscreteIoExpander::enableBitwiseInversion()
/// \sa I2cDiscreteIoExpander::disableBitwiseInversion()
bool PCF8575::isInverted() {
	return shouldInvert_;
}
