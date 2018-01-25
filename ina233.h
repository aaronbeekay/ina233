/*
 ina233.h: library for interfacing with TI INA233 shunt current/voltage monitor
 Aaron Bonnell-Kangas, January 2018
 License TBD - all rights reserved for now.
 */

#ifndef ina233_h
#define def_h

#include "Arduino.h"

class ina233
{
	public:
		ina233(int address, double Rshunt, double Imax);		//TODO init options
	 
		int			readBusVoltageCode();
		int			readShuntVoltageCode();
		int			readPowerCode();
		int			readCurrentCode();
		uint16_t 	readADCConfiguration();
		
		double		readBusVoltage();
		double		readShuntVoltage();
		double		readPower();
		double		readCurrent();

		uint32_t	lastAccumulatorReadTime;		// micros() output from last accum read
		int32_t		lastAccumulatorRead;			// last value read from power accumulator
		uint32_t	lastAccumulatorSamples;			// number of samples reported in last accumulator read
			
		double		lastAveragePower;
		double		lastEnergy;
		
		void		configureADC(char avgMode, char busConvTime, char shuntConvTime );
		void		configureShuntValue( double Rshunt, double Imax );
		void		clearFaults();
		void		clearAccumulator();
		void		readAccumulator();
		
	private:
		int			_readTwoByteRegister( int register );
		int			_address;
		double		_Rshunt;						// value of shunt resistor
		double		_Imax;							// full-scale current chosen
		double		_currentLSB;
		double		_powerLSB;
		float		_inaBusVoltageSamplePeriod;
		float		_inaShuntVoltageSamplePeriod;
		// TODO configuration settings
};

// Register addresses
int const CLEAR_FAULTS			= 0x03;
int const RESTORE_DEFAULT_ALL	= 0x12;
int const CAPABILITY			= 0x19;
int const IOUT_OC_WARN_LIMIT	= 0x4A;
int const VIN_OV_WARN_LIMIT		= 0x57;
int const VIN_UV_WARN_LIMIT		= 0x58;
int const PIN_OP_WARN_LIMIT		= 0x6B;
int const STATUS_BYTE			= 0x78;
int const STATUS_WORD			= 0x79;
int const STATUS_IOUT			= 0x7B;
int const STATUS_INPUT			= 0x7C;
int const STATUS_CML			= 0x7E;
int const STATUS_MFR_SPECIFIC	= 0x80;
int const READ_EIN				= 0x86;
int const READ_VIN				= 0x88;
int const READ_IIN				= 0x89;
int const READ_PIN				= 0x97;
int const MFR_ADC_CONFIG		= 0xD0;
int const MFR_READ_VSHUNT		= 0xD1;
int const MFR_ALERT_MASK		= 0xD2;
int const MFR_CALIBRATION		= 0xD4;
int const MFR_DEVICE_CONFIG		= 0xD5;
int const CLEAR_EIN				= 0xD6;

// Scaling factors
double const VIN_LSB_VALUE_V	= 0.00125;
double const VSHUNT_LSB_VALUE_V = 0.0000025;

#endif