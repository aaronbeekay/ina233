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

void configureShuntValue( ?? )
{

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

