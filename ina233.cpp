/*
 ina233.h: library for interfacing with TI INA233 shunt current/voltage monitor
 Aaron Bonnell-Kangas, January 2018
 License TBD - all rights reserved for now.
 */

#include "Arduino.h"
#include "ina233.h"
#include "Wire.h"

ina233::ina233(int address)
{
	_address = address;
	Wire.begin();
}

int readBusVoltageCode()
{
	int code = _readTwoByteRegister( READ_VIN );
	return code;
}

int readShuntVoltageCode()
{
	int code = _readTwoByteRegister( MFR_READ_VSHUNT );
	return code;
}

int readPowerCode()
{
	int code = _readTwoByteRegister( READ_PIN );
	return code;
}

int readCurrentCode()
{
	int code = _readTwoByteRegister( READ_IIN );
	return code;
}

double readPower()
{
	int code = readPowerCode();
	// TODO set scaling appropriately by current config
	double power = 0.0;
	
	return power;
}

double readBusVoltage()
{
	int code = readBusVoltageCode();
	double busVoltage = code * VIN_LSB_VALUE_V;
	
	return busVoltage;
}

double readShuntVoltage()
{
	int code = readShuntVoltageCode();
	double shuntVoltage = code * VSHUNT_LSB_VALUE_V;
	
	return shuntVoltage;
}

double readCurrent()
{
	int code = readCurrentCode();
	// TODO set scaling appropriately by current config
	double current = 0.0;
	
	return current;
}

void configureADC(char avgMode, char busConvTime, char shuntConvTime )
{
	// Read current ADC settings
	int settings = 0; 	// todo
	settings = settings & 0b0000000000000111 	// want to keep power mode the same
	
	settings |= (shuntConvTime & 0b111) << 3;
	settings |= (busConvTime & 0b111) << 6;
	settings |= (avgMode & 0b111) << 9;
	
	// TODO write settings back to device
}

void clearFaults()
{
	// TODO
}

/* 
	Automagically configure the MFR_CALIBRATION register that sets the scaling factor inside the INA233.
	Arguments:
		Rshunt: value of the shunt resistor connected to the device, in ohms
		Imax: maximum expected current, in amps (single-sided, so +-150A measurement -> Imax = 150)
	This function does the math internally and sets the configuration register according to TI's datasheet recommendations. If you want full control over the exact calibration value to be set, you should do the math offline and set the register with your own static value.
 */
void configureShuntValue( double Rshunt, double Imax )
{
	_currentLSB = Imax/32768;
	_powerLSB = _currentLSB*25;							// fixed inside INA233
	int cal = (int)(0.00512/(_currentLSB*Rshunt));		// per TI datasheet
	
	// Write value to cal register
	Wire.beginTransmission( _address );
	Wire.write( MFR_CALIBRATION );
	Wire.write( (uint8_t)(cal & 0x00FF) );				// write LSB
	Wire.write( (uint8_t)((cal & 0xFF00)>>8) );			// write MSB
	Wire.endTransmission();
	
	//TODO read the value back lol
	
}

int _readTwoByteRegister( int register )
{
	int code = 0;
	
	Wire.beginTransmission( _address );			// send dev address
	Wire.write( register );						// send register
	Wire.endTransmission( false );				// repeated START
	Wire.requestFrom( _address, 2, true );		// read data
	
	while( !Wire.available() ){ yield(); };		// wait for reply
	code += Wire.read();						// LSB is sent first
	while( !Wire.available() ){ yield(); };		// wait for reply
	code += Wire.read() << 8;					// then MSB
												// TODO: what if it doesn't reply?
	return code;
}

