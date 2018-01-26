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
	
	// Check what the current direction is now, set the new EIN_ACCUM appropriately
	int16_t iinCode = readCurrentCode();
	if( iinCode > 0 ) {
		setEINDirection( EIN_DIRECTION_POSITIVE );
	} else if( iinCode < 0 ) {
		setEINDirection( EIN_DIRECTION_NEGATIVE );
	}
	// If the current is zero we don't really know what to do, so don't do anything.
	
	// Initialize our energy counter and duration timer
	clearAccumulator;
	lastAccumulatorReadTime = micros();
	totalEnergy = 0;
	
	// temp
	_inaBusVoltageSamplePeriod = 0.0011;	// INA233 default 1.1ms
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

void ina233::setEINDirection( uint8_t einDirection )
{
	// Read the current MFR_DEVICE_CONFIG settings
	uint8_t mfr_device_config;
	mfr_device_config = _readOneByteRegister( MFR_DEVICE_CONFIG );
	
	// Change the EIN_ACCUM bits (and update our local flag to match)
	if( einDirection == EIN_DIRECTION_POSITIVE ) {
		mfr_device_config &= 0b11001111;
		mfr_device_config |= (EIN_DIRECTION_POSITIVE << 5);
		_einDirection = EIN_DIRECTION_POSITIVE;
	} else if( einDirection == EIN_DIRECTION_NEGATIVE ){
		mfr_device_config &= 0b11001111;
		mfr_device_config |= (EIN_DIRECTION_NEGATIVE << 5);
		_einDirection = EIN_DIRECTION_NEGATIVE;
	}
	
	// Write it back to the device
	_writeOneByteRegister( MFR_DEVICE_CONFIG, mfr_device_config );
}

/* Read the EIN_STATUS bit from the MFR_DEVICE_CONFIG register and return its value */
uint8_t ina233::readEINStatus()
{
	uint8_t einStatus = 0;
	
	// Read MFR_DEVICE_CONFIG
	einStatus = _readOneByteRegister( MFR_DEVICE_CONFIG );
	
	// Mask EIN_STATUS bit
	einStatus = einStatus & 0b10000000;
	einStatus = einStatus >> 7;
	
	// 1 = power direction has changed since EIN_ACCUM last cleared, 0 = has not changed
	return einStatus;
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
	uint32_t 	durationTicks;
	double		averagePower;
	double		energy;
	double 		duration;
	
	/* Get the data off the INA233 and set a micros() timestamp 
	 */
	Wire.beginTransmission( _address );
	Wire.write( READ_EIN );
	Wire.endTransmission( false );				// repeated START
	Wire.requestFrom( _address, 7, true );
	
	accumulatorReadTime = micros();				// save the timestamp here - not quite
												//	sure when the INA233 commits the data
												//	but this is a good compromise for now
						
	
	while( !Wire.available() ){ yield(); }; 	// wait for reply
	Wire.read();								// INA233 prefixes every accumulator read
												// with the number "6", irritatingly.
												// trash this byte
												
	// First three bytes: power accumulator value and overflow count				 
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
	
	/* POWER DIRECTION HANDLING
	 *	The power accumulator in INA233 is unsigned: it only counts up. It's on us
	 *	to decide whether we increase or decrease our energy accumulator for a 
	 * 	given reading from the device.
	 *
	 *	To deal with this, put the INA233 in one-direction accumulator mode (sum
	 * 	negative or positive samples only), and check each read if the direction has
	 * 	changed (check EIN_STATUS). If it has changed, we throw away the accumulator
	 * 	value from this read and flip the INA233 to the direction matching the, uh,
	 * 	current current. 
	 *
	 * 	IMPORTANT: This means that we lose some energy information every time the 
	 * 	current direction changes. For the storage battery use case, this isn't terribly
	 * 	important, because direction changes happen infrequently enough that it shouldn't
	 * 	affect the summed energy significantly. But if your power profile has frequent, 
	 * 	brief, bidirectional power pulses, this might matter more to you. You would
	 * 	probably want to increase the frequency of accumulator reads, but I have declared
	 * 	this problem out of my scope. Godspeed.
	 */
	 if( readEINStatus() == 1 ){
	 	// Power direction has changed, accumulator contents invalid.
	 	
	 	// Check what the current direction is now, set the new EIN_ACCUM appropriately
	 	int16_t iinCode = readCurrentCode();
	 	if( iinCode > 0 ) {
	 		setEINDirection( EIN_DIRECTION_POSITIVE );
	 	} else if( iinCode < 0 ) {
	 		setEINDirection( EIN_DIRECTION_NEGATIVE );
	 	}
	 	// If the current is zero we don't really know what to do, so don't do anything.
	 	
		clearAccumulator();
		clearEINStatus();
	 	lastAccumulatorReadTime = accumulatorReadTime;
	 	return;
	 } 	
	
	/* Calculate duration of this measurement based on Arduino clock
	 * 	(TI says that INA233 internal oscillator variance is too high for 
	 * 	accurate energy measurement) 
	 */
	if( accumulatorReadTime < lastAccumulatorReadTime ){
		//micros() wrapped
		durationTicks = accumulatorReadTime + (0xFFFFFFFF - lastAccumulatorReadTime);
	} else {
		durationTicks = accumulatorReadTime - lastAccumulatorReadTime;
	}
	duration = (double)durationTicks * 0.000001;
	
	/* DEBUG: compare INA233 clock to Arduino clock */
	double intervalINA = (double)(sampleCount) * (_inaBusVoltageSamplePeriod + _inaShuntVoltageSamplePeriod);
	Serial.print("Arduino interval: ");
	Serial.print(duration, DEC);
	Serial.print(" INA interval: ");
	Serial.print(sampleCount, DEC);
	Serial.print("/");
	Serial.print(intervalINA, DEC);
	Serial.print("\nError: ");
	Serial.print(duration - intervalINA, DEC);
	Serial.print(" sec\n");
	/* END DEBUG */
	
	// Convert values to real-world units
	averagePower = (double)powerAccumulator;	// cast to double first to avoid mult 
												// overflow
	averagePower = averagePower * _powerLSB;
	averagePower = averagePower / sampleCount;
	
	energy = averagePower * duration;

	lastAccumulatorReadTime = accumulatorReadTime;
	lastAveragePower = averagePower;
	lastEnergy = energy;
	
	if( _einDirection == EIN_DIRECTION_POSITIVE) {
		totalEnergy += energy;
	} else if( _einDirection == EIN_DIRECTION_NEGATIVE) {
		totalEnergy -= energy;
	}
}

void ina233::clearEINStatus()
{
	// It is not presently clear whether the EIN_STATUS bit is cleared when the accumulator is reset.
	// TODO: determine this behavior and implement bit-clear (write 0 to MFR_DEVICE_CONFIG[7]) if necessary
	return;
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
	
	_writeTwoByteRegister( MFR_CALIBRATION, cal );
	
	//TODO read the value back lol
	
}

void ina233::_writeOneByteRegister( uint8_t reg, uint8_t value )
{
	Wire.beginTransmission( _address );
	Wire.write( reg );
	Wire.write( value );		
	Wire.endTransmission();
}

void ina233::_writeTwoByteRegister( uint8_t reg, uint16_t value )
{
	Wire.beginTransmission( _address );
	Wire.write( reg );
	Wire.write( (uint8_t)(value & 0x00FF) );		// write LSB
	Wire.write( (uint8_t)((value & 0xFF00)>>8) );	// write MSB
	Wire.endTransmission();
}

int ina233::_readTwoByteRegister( int reg )
{
	int code = 0;
	
	Wire.beginTransmission( _address );			// send dev address
	Wire.write( reg );							// send register
	Wire.endTransmission( false );				// repeated START
	Wire.requestFrom( _address, 2, true );		// read data
	
	while( !Wire.available() ){ yield(); }; 	// wait for reply
	code += Wire.read();						// LSB is sent first
	while( !Wire.available() ){ yield(); }; 	// wait for reply
	code += Wire.read() << 8;					// then MSB
						
	// TODO: what if it doesn't reply?
	
	return code;
}

uint8_t ina233::_readOneByteRegister( uint8_t reg )
{
	uint8_t code;
	
	Wire.beginTransmission( _address );			// send dev address
	Wire.write( reg );							// send register
	Wire.endTransmission( false );				// repeated START
	Wire.requestFrom( _address, 1, true );		// read data
	
	while( !Wire.available() ){ yield(); }; 	// wait for reply
	code = Wire.read();
	
	return code;
}
