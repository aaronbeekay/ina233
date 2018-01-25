/*
 ina233.h: library for interfacing with TI INA233 shunt current/voltage monitor
 Aaron Bonnell-Kangas, January 2018
 License TBD - all rights reserved for now.
 */

#include "Arduino.h"
#include "ina233.h"
#include "Wire.h"

ina233::ina233(int address, double Rshunt, double Imax)
{
	_address = address;
	Wire.begin();
	
	// temp
	_inaBusVoltageSamplePeriod = 0.0011;		// INA233 default 1.1ms
	_inaShuntVoltageSamplePeriod = 0.0011;	// INA233 default 1.1ms
}

int ina233::readBusVoltageCode()
{
	int code = _readTwoByteRegister( READ_VIN );
	return code;
}

int ina233::readShuntVoltageCode()
{
	int code = _readTwoByteRegister( MFR_READ_VSHUNT );
	return code;
}

int ina233::readPowerCode()
{
	int code = _readTwoByteRegister( READ_PIN );
	return code;
}

int ina233::readCurrentCode()
{
	int code = _readTwoByteRegister( READ_IIN );
	return code;
}

double ina233::readPower()
{
	int code = readPowerCode();
	double power = code * _powerLSB;
	
	return power;
}

double ina233::readBusVoltage()
{
	int code = readBusVoltageCode();
	double busVoltage = code * VIN_LSB_VALUE_V;
	
	return busVoltage;
}

double ina233::readShuntVoltage()
{
	int code = readShuntVoltageCode();
	double shuntVoltage = code * VSHUNT_LSB_VALUE_V;
	
	return shuntVoltage;
}

double ina233::readCurrent()
{
	int code = readCurrentCode();
	double current = code * _currentLSB;
	
	return current;
}

void ina233::configureADC(char avgMode, char busConvTime, char shuntConvTime )
{
	// TODO Read current ADC settings
	int settings = 0;		// todo
	settings = settings & 0b0000000000000111;		// want to keep power mode the same
	
	settings |= (shuntConvTime & 0b111) << 3;
	settings |= (busConvTime & 0b111) << 6;
	settings |= (avgMode & 0b111) << 9;
	
	// TODO write settings back to device
}

uint16_t ina233::readADCConfiguration()
{
	uint16_t reg = _readTwoByteRegister( MFR_ADC_CONFIG );
	/*
	//debug
	Serial.print("ADC conf: ");
	Serial.print(reg, BIN);
	Serial.print("\n");
	*/

	return reg;
}

/*
	Update the instance's energy counter (lastAccumulatorReadTime, etc). Clear the INA233 accumulator.
	
	This function uses Arduino's micros() for time tracking since the INA233 datasheet warns that the internal oscillator variance can result in "energy error up to 10%". Scary.
	
	No return values, read what you care about using the <TBD> methods. 
 */
void ina233::readAccumulator()
{
	uint32_t	powerAccumulator;
	uint32_t	sampleCount;
	uint32_t	accumulatorReadTime;
	double	averagePower;
	double	averageEnergy;
	
	Wire.beginTransmission( _address );
	Wire.write( READ_EIN );
	Wire.endTransmission( false );		// repeated START
	Wire.requestFrom( _address, 7, true );
	
	accumulatorReadTime = micros();		// save the timestamp here - not quite
										//	sure when the INA233 commits the data
										//	but this is a good compromise for now
						
	// First three bytes: power accumulator value and overflow count
	while( !Wire.available() ){ yield(); }; 	// wait for reply
	powerAccumulator = Wire.read();				// junk byte - trash it?
	Serial.print("this should be 6: ");
	Serial.print(powerAccumulator, DEC);
	Serial.print("\n");	 
					 
	while( !Wire.available() ){ yield(); }; 	// wait for reply
	powerAccumulator = Wire.read();				// accumulator LSB
	while( !Wire.available() ){ yield(); }; 	// wait for next byte
	powerAccumulator += Wire.read() << 8;		// accumulator MSB
	while( !Wire.available() ){ yield(); }; 	// wait for next byte
	powerAccumulator += Wire.read() << 16;		// accumulator rollover count
	
	// Next three bytes: sample counter value 
	while( !Wire.available() ){ yield(); }; 	// wait for reply
	sampleCount = Wire.read();					// sample count low byte
	while( !Wire.available() ){ yield(); }; 	// wait for next byte
	sampleCount += Wire.read() << 8;			// sample count mid byte
	while( !Wire.available() ){ yield(); }; 	// wait for next byte
	sampleCount += Wire.read() << 16;			// sample count high byte 
	
	clearAccumulator();
	
	// Convert values to real-world units
	averagePower = (double)powerAccumulator;	//cast to double first to avoid mult overflow
	averagePower = averagePower * _powerLSB;	// UMMM does this make sense?
	
	// debug: compare INA233 clock to Arduino clock
	double intervalArduino = (double)(accumulatorReadTime - lastAccumulatorReadTime) * 0.000001;
	double intervalINA = (double)(sampleCount) * (_inaBusVoltageSamplePeriod + _inaShuntVoltageSamplePeriod);
	Serial.print("Arduino interval: ");
	Serial.print(intervalArduino, DEC);
	Serial.print(" INA interval: ");
	Serial.print(sampleCount, DEC);
	Serial.print("/");
	Serial.print(intervalINA, DEC);
	Serial.print("\nError: ");
	Serial.print(intervalArduino - intervalINA, DEC);
	Serial.print(" sec\n");
	// end debug

	lastAccumulatorReadTime = accumulatorReadTime;
	
}

void ina233::clearAccumulator()
{
	Wire.beginTransmission( _address );
	Wire.write( CLEAR_EIN );
	Wire.endTransmission();
	return;
}

void ina233::clearFaults()
{
	// TODO
	return;
}

/* 
	Automagically configure the MFR_CALIBRATION register that sets the scaling factor inside the INA233.
	Arguments:
	Rshunt: value of the shunt resistor connected to the device, in ohms
	Imax: maximum expected current, in amps (single-sided, so +-150A measurement -> Imax = 150)
	This function does the math internally and sets the configuration register according to TI's datasheet recommendations. If you want full control over the exact calibration value to be set, you should do the math offline and set the register with your own static value.
 */
void ina233::configureShuntValue( double Rshunt, double Imax )
{
	_currentLSB = Imax/32768;
	_powerLSB = _currentLSB*25;				// fixed inside INA233
	int cal = (int)(0.00512/(_currentLSB*Rshunt));	// per TI datasheet
	
	// Write value to cal register
	Wire.beginTransmission( _address );
	Wire.write( MFR_CALIBRATION );
	Wire.write( (uint8_t)(cal & 0x00FF) );		// write LSB
	Wire.write( (uint8_t)((cal & 0xFF00)>>8) );		// write MSB
	Wire.endTransmission();
	
	//TODO read the value back lol
	
}

int ina233::_readTwoByteRegister( int reg )
{
	int code = 0;
	
	Wire.beginTransmission( _address );		// send dev address
	Wire.write( reg );			// send register
	Wire.endTransmission( false );		// repeated START
	Wire.requestFrom( _address, 2, true );	// read data
	
	while( !Wire.available() ){ yield(); }; // wait for reply
	code += Wire.read();				// LSB is sent first
	while( !Wire.available() ){ yield(); }; // wait for reply
	code += Wire.read() << 8;			// then MSB
						// TODO: what if it doesn't reply?
	return code;
}
