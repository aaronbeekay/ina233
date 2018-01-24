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
	int code = 0;
	
	Wire.beginTransmission( _address );			// send dev address
	Wire.write( READ_VIN );						// send register
	Wire.endTransmission( false );				// repeated START
	Wire.requestFrom( _address, 2, true );		// read data
	
	while( !Wire.available() ){ yield(); };		// wait for reply
	code += Wire.read();						// LSB is sent first
	while( !Wire.available() ){ yield(); };		// wait for reply
	code += Wire.read() << 8;					// then MSB
												// TODO: what if it doesn't reply?
	return code;
}

int readShuntVoltageCode()
{
	int code = 0;
	
	Wire.beginTransmission( _address );			// send dev address
	Wire.write( MFR_READ_VSHUNT );				// send register
	Wire.endTransmission( false );				// repeated START
	Wire.requestFrom( _address, 2, true );		// read data
	
	while( !Wire.available() ){ yield(); };		// wait for reply
	code += Wire.read();						// LSB is sent first
	while( !Wire.available() ){ yield(); };		// wait for reply
	code += Wire.read() << 8;					// then MSB
												// TODO: what if it doesn't reply?
	return code;
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