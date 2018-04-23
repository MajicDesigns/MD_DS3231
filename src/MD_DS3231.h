/**
\mainpage Arduino DS3231 Library
The Maxim DS3231 Real Time Clock IC
-----------------------------------
The DS3231 is a low-cost, extremely accurate I2C real time clock (RTC) with an 
integrated temperature compensated crystal oscillator (TCXO) and crystal.

Microprocessor interface is exclusively through an I2C bidirectional bus.

The device 
- incorporates a battery input, and maintains accurate timekeeping when main power 
to the device is interrupted. 
- maintains seconds, minutes, hours, day, date, month, and year information. 
- automatically adjusts dates for months with fewer than 31 days, including 
corrections for leap year. 
- operates in either the 24-hour or 12-hour format with an AM/PM indicator.
- includes two programmable time-of day alarms.
- has and frequency programmable square-wave output.

Precision is maintained through a temperature-compensated voltage reference
and comparator circuit monitors the status of Vcc to detect power failures 
and to automatically switch to the backup supply when necessary.

This library features access to all on-chip features
- Read and write clock time registers
- Read and write alarm registers and controls
- Read/write clock and alarm registers as RAM
- Control of square wave generator (on/off & frequency)
- Control of clock features (Square Wave type and on/off, 12/24H, day of week, etc)

___

\subpage pageSoftware

___

Revision History 
----------------
Apr 2018 version 1.2.3
- Small fixes to code

Feb 2017 version 1.2.2
- Fixed text in header file

May 2017 version 1.2
- Removed deprecated language features warnings for examples
- Additional comments for examples

Apr 2016 version 1.1
- Fixed bug when setting time in 12 hour mode
- Added now() and isrunning() methods for compatibility with MD_DS1307 library

Aug 2015 version 1.0
- Initial version created from basic MD_1307 Library

Copyright
---------
Copyright (C) 2015 Marco Colli. All rights reserved.

This library is free software; you can redistribute it and/or
modify it under the terms of the GNU Lesser General Public
License as published by the Free Software Foundation; either
version 2.1 of the License, or (at your option) any later version.

This library is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
Lesser General Public License for more details.

You should have received a copy of the GNU Lesser General Public
License along with this library; if not, write to the Free Software
Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301 USA

\page pageSoftware Software Overview
Using the Library
-----------------

The library has a simple interface to the RTC hardware implemented through:
- a set of time (h, m, s and dow) and date (yyyy, mm, dd) variables. All data to and from
the RTC passes through these. Clock or alarm time data is read or written to these interface
registers before the appropriate methods are invoke to act on that data.
- control() and status() methods to set and query the functionality of the RTC.
- specific functions to set and test for each of the two alarms.

Alarms may be operated in either polled mode or interrupt mode, as described below.

___

The Control and Status Methods
----------------------------------
The control() and status() methods are the main interface to action setting parameters or 
status values. The parameters used for each are similar, but not symmetrical, due
to the way the hardware is implemented. The table below highlights the combinations of valid 
parameters for the control() method and whether a parameter can be changed by user code.

The status() method will return the current value from the parameter nominated and is guaranteed to
be in the valid set.

 Function            |R|W| Write value
---------------------|-|-|-------------------------------------------------------------------|
DS3231_CLOCK_HALT    |Y|Y| DS3231_ON, DS3231_OFF 
DS3231_SQW_ENABLE    |Y|Y| DS3231_ON, DS3231_OFF
DS3231_SQW_TYPE      |Y|Y| DS3231_SQW_1HZ, DS3231_SQW_1KHZ, DS3231_SQW_4KHZ, DS3231_SQW_8KHZ
DS3231_12H           |Y|Y| DS3231_ON, DS3231_OFF 
DS3231_TCONV         |Y|Y| DS3231_ON, DS3231_OFF
DS3231_INT_ENABLE    |Y|Y| DS3231_ON, DS3231_OFF
DS3231_A1_INT_ENABLE |Y|Y| DS3231_ON, DS3231_OFF
DS3231_A2_INT_ENABLE |Y|Y| DS3231_ON, DS3231_OFF
DS3231_HALTED_FLAG   |Y|Y| DS3231_OFF
DS3231_32KHZ_ENABLE  |Y|Y| DS3231_ON, DS3231_OFF
DS3231_BUSY_FLAG     |Y|N| N/A
DS3231_A1_FLAG       |Y|Y| DS3231_OFF
DS3231_A2_FLAG       |Y|Y| DS3231_OFF
DS3231_AGING_OFFSET  |Y|Y| 0x00 - 0xff

___

Working with the Current Time
-----------------------------
__Reading__ the current time from the clock is a call to the readTime() method. The current date 
and time is then available in the interface registers.

__Writing__ the current time is a sequence of writing to the interface registers followed by a call 
to the writeTime() method.

___

Working with Alarms
-------------------
__Writing__ the alarm trigger time data is achieve by writing to the interface registers followed
by a call to the setAlarm1() or setAlarm2() method. The alarm type is specified when the method is invoked, 
and is one of the almType_t values, noting that values are specific to alarm 1 and alarm 2.

__Reading__ the alarm trigger set is by invoking the getAlarm1() or getAlarm2() methods to load the interface 
registers, from which the data can be read. The alarm trigger type is obtained separately using the 
getAlarm1Type() or getAlarm2Type() methods.

__Checking__ if an alarm has triggered can be done in two ways, depending on whether it is a polled alarm or 
interrupts have been enabled.

- _Polled Alarms_ are implemented by calling the checkAlarm1() or checkAlarm2() method. If a callback function
is defined (using setAlarm1Callback() or setAlarm2Callback()) the callback function will be invoked. These methods 
encapsulate a check for the alarm trigger, invoke the callback function if triggered and reset the alarm flag, 
effectively providing an interrupt-like operation without the restrictions of being in an interrupt routine. If 
no callback function is defined, the return status from the checkAlarm1() or checkAlarm2() should be used instead.

- _Interrupt Alarms_ are configured by invoking the control() method to enable interrupts (INT_ENABLE), followed 
by enabling the interrupt for the required alarm (A1_INT_ENABLE or A2_INT_ENABLE). The alarm flag (control() with 
A1FLAG or A2_FLAG) should be rest to initialise a 'zero' start for the interrupt cycles. Once the interrupt cycle has 
completed, the alarm flag must be reset to re-enable the interrupt (control() with parameter A1_FLAG or A2_FLAG).
The interrupt handler must be set up externally from the library and the hardware properly configured to accept the 
interrupt. Note, however:
 + the interrupt line from the DS3231 will be held logic HIGH and the interrupt is at logic LOW, so
a HIGH to LOW transition (FALLING) should be specified as the interrupt trigger.
 + during the interrupt there can be no I2C communications as this uses interrupts. So checking a RTC
 status in the Interrupt Service Routine (ISR) is not possible.

The DS3231_LCD_Time example has examples of the different ways of interacting with the RTC.
 */
  
#ifndef MD_DS3231_h
#define MD_DS3231_h

#include <Arduino.h>
/**
 * \file
 * \brief Main header file for the MD_DS3231 library
 */

/**
 \def ENABLE_TEMP_COMP
 Set to 1 (default) to enable the temperature comparator related
 functions. The temperature is returned as a floating point number -
 excluding the function(s) may provide some memory gains.
 */
#define ENABLE_TEMP_COMP  1   ///< Enable temperature compensation functions

/**
  Control and Status Request enumerated type.
  *
  * This enumerated type is used with the control() and status() methods to identify 
  * the control action request.
  */
enum codeRequest_t  
{
 DS3231_CLOCK_HALT,   ///< Controls the EOSC bit. When set to ON, the oscillator is stopped when the DS3231
                      ///< switches to battery, effectively stopping any RTC functions. This bit is OFF when 
                      ///< power is first applied. When the DS3231 is powered by Vcc, the oscillator is always on 
                      ///< regardless of this status. 
 DS3231_SQW_ENABLE,   ///< Controls the BBSQW bit. This bit is OFF when power is first applied. When 
                      ///< set to ON with INT_ENABLE OFF, the square wave output is enabled.
 DS3231_SQW_TYPE,     ///< Controls the RS1 and RS2 bits to control the frequency of the square wave output. When
                      ///< power is first applied SQW_8KHZ is selected.
 DS3231_12H,          ///< Controls whether the library is using 24 hour time or AM/PM designation. Setting ON enables
                      ///< 12 hour time and AM/PM.
 DS3231_TCONV,        ///< Controls the CONV bit. Setting this ON  forces the temperature sensor to convert the temperature
                      ///< into digital code and execute the TCXO algorithm. The user should check the status of BUSY_FLAG
                      ///<  before forcing the controller to start a new TCXO execution. The TCONV status remains ON from 
                      ///< the time it is written until the conversion is finished, at which time both TCONV and BUSY_FLAG go 
                      ///< OFF. The TCONV status should be  used when monitoring the status of a user-initiated conversion.
 DS3231_INT_ENABLE,   ///< Controls the INTCN bit. When set OFF, a square wave is output on the INT/SQW pin. When set 
                      ///< ON, then the Alarm interrupt functions are enabled if the corresponding alarm interrupt is also 
                      ///< enabled. The corresponding An_FLAG is always set regardless of this status. The function is ON 
                      ///< when power is first applied.
 DS3231_A1_INT_ENABLE,///< Controls the A1E bit. When ON, the this bit enables the A1_FLAG bit to generate an interrupt when 
                      ///< INT_ENABLE is also ON. When this is OFF, or INT_ENABLE is OFF, no interrupt signal is generated 
                      ///< for Alarm 1. A1_INT_ENABLE is OFF when power is first applied.
 DS3231_A2_INT_ENABLE,///< Controls the A2E bit and operates like A1_INT_ENABLE but for alarm 2.
 DS3231_HALTED_FLAG,  ///< Controls the OSF bit. When ON, this indicates that the oscillator either is stopped or was stopped 
                      ///< for some period and may be used to judge the validity of the timekeeping data. It remains ON 
                      ///< turned OFF by user code.
 DS3231_32KHZ_ENABLE, ///< Controls the EN32KHZ bit. When set to ON, the 32kHz pin is enabled and outputs a 32.768kHz 
                      ///< square wave signal. When set OFF, the 32kHz pin goes to a high-impedance state. The initial 
                      ///< power-up state is ON.
 DS3231_BUSY_FLAG,    ///< Reports the BSY bit. This status indicates the device is busy executing TCXO functions. It goes 
                      ///< ON when the conversion is being carried out and then set to OFF when the TCXO is in the idle state.
 DS3231_A1_FLAG,      ///< Controls the A1F bit. When ON, it indicates that the time matched the alarm 1 registers. If A1_FLAG 
                      ///< is ON and the INT_ENABLE status IS ON, an interrupt is also generated for the alarm. A1_FLAG is only
                      ///< cleared when written OFF by the user code.
 DS3231_A2_FLAG,      ///< Controls the A2F bit and operates like the A1_FLAG except for alarm 2.
 DS3231_AGING_OFFSET, ///< Controls the Aging Offset register value. The register takes a user-provided value to add to or subtract 
                      ///< from the codes in the capacitance array registers. The code is encoded in two's complement, with bit 7 
                      ///< representing the sign bit.
};

/**
  * Control and Status Request return values enumerated type.
  *
  * This enumerated type is used as the return status from the
   * control() and status() methods.
  */
enum codeStatus_t  
{
 DS3231_ERROR,    ///< An error occurred executing the requested action
 DS3231_ON,       ///< Represents an ON status to set or returned from a get
 DS3231_OFF,      ///< Represents an OFF status to set or returned from a get
 DS3231_SQW_1HZ,  ///< Set or get 1Hz square wave specifier for SQW_TYPE parameter
 DS3231_SQW_1KHZ, ///< Set or get 1kHz square wave specifier for SQW_TYPE parameter
 DS3231_SQW_4KHZ, ///< Set or get 4kHz square wave specifier for SQW_TYPE parameter
 DS3231_SQW_8KHZ, ///< Set or get 8kHz square wave specifier for SQW_TYPE parameter
}; 

// Device parameters
#define	DS3231_RAM_MAX		19	///< Total number of RAM registers that can be read from the device

/**
  * Alarm Type specifier enumerated type.
  *
  * This enumerated type is used to set and inspect the alarm types
   * for Alarms 1 and 2 using the setAlarmType() and getAlarmType() methods.
  */
enum almType_t 
{
 DS3231_ALM_ERROR,   ///< An error occurred executing the requested action
 DS3231_ALM_SEC,     ///< Alarm once per second (alm 1 only)
 DS3231_ALM_S,       ///< Alarm when seconds match (alm 1 only)
 DS3231_ALM_MIN,     ///< Alarm once per minute (alm 2 only)
 DS3231_ALM_M,       ///< Alarm when minutes match (alm 2 only)
 DS3231_ALM_MS,      ///< Alarm when minutes and seconds match (alm 1 only)
 DS3231_ALM_HM,      ///< Alarm when hours and minutes match (alm 2 only)
 DS3231_ALM_HMS,     ///< Alarm when hours, minutes and seconds match (alm 1 only)
 DS3231_ALM_DTHM,    ///< Alarm when date, hours and minutes match (alm 2 only)
 DS3231_ALM_DTHMS,   ///< Alarm when date, hours, minutes and seconds match (alm 1 only)
 DS3231_ALM_DDHM,    ///< Alarm when day, hours and minutes match (alm 2 only)
 DS3231_ALM_DDHMS,   ///< Alarm when day, hours, minutes and seconds match (alm 1 only)
};

/**
 * Core object for the MD_DS3231 library
 */
class MD_DS3231
{
  public:
    
 /** 
  * Class Constructor
  *
  * Instantiate a new instance of the class. One instance of the class is 
  * created in the libraries as the RTC object.
  * 
  */
  MD_DS3231();
  
 //--------------------------------------------------------------
 /** \name Methods for object and hardware control.
  * @{
  */
 /** 
  * Set the control status of the specified parameter to the specified value.
  *
  * The device has a number of control parameters that can be set through this method. 
  * The type of control action required is passed through the mode parameter and 
  * should be one of the control actions defined by codeRequest_t. The value that 
  * needs to be supplied on the control action required is one of the defined 
  * actions in codeStatus_t or a numeric parameter suitable for the control action.
  * Not all combinations of item and value are valid.
  *
  * \sa Software Overview section in the introduction for a table of valid combinations.
  *
  * \param item  one of the codeRequest_t values.
  * \param value one codeStatus_t values.
  * \return false if parameter errors, true otherwise.
  */
  boolean control(codeRequest_t item, uint8_t value);

 /** 
  * Obtain the current setting for the specified parameter.
  * 
  * Any of the parameters that can be set from the control() method can be queried using
  * this method. The codeStatus_t value returned will be one of the valid values for the 
  * control() method for each specific codeRequest_t parameter.
  *
  * \sa Software Overview section in the introduction for a table of valid combinations.
  *
  * \param item  one of the codeRequest_t values.
  * \return codeStatus_t value setting or DS3231_ERROR if an error occurred.
  */
  codeStatus_t status(codeRequest_t item);

  /** @} */

 //--------------------------------------------------------------
 /** \name Methods for RTC operations
  * @{
  */

 /**
  * Read the current time into the interface registers
  *
  * Query the RTC for the current time and load that into the library interface registers 
  * (yyyy, mm, dd, h, m, s, dow, pm) from which the data can be accessed.
  *
  * \sa setCentury() method.
  *
  * \return false if errors, true otherwise.
  */
  boolean readTime(void);

 /**
  * Write the current time from the interface registers
  *
  * Write the data in the interface registers (yyyy, mm, dd, h, m, s, dow, pm) 
  * as the current time in the RTC.
  *
  * \sa setCentury() method.
  *
  * \return false if errors, true otherwise.
  */
  boolean writeTime(void);

 /**
  * Set the current century for year handling in the library
  *
  * The RTC only stores the last 2 digits of the year and has a century indicator.
  * This means in practice that dates in a range of 199 years from a base date 
  * can be represented by the library. This method allows user code to set a base 
  * century for date handling in the library. The default for this value in the libraries
  * is 20, which allows dates in the range from 2000 (20*100) to 2199.
  *
  * \sa getCentury() method
  *
  * \param   c the year base century. Dates will start from (c*100).
  * \return false if errors, true otherwise.
  */
  inline boolean setCentury(uint8_t c) { _century = c; };

 /**
  * Get the current century for year handling in the library
  *
  * Return the current century library setting.
  *
  * \sa getCentury() method for an explanation of year handling
  *
  * \return the year base century.
  */
  inline uint8_t getCentury(void) { return(_century); };

 /**
  * Compatibility function - Read the current time
  *
  * Wrapper to read the current time.
  *
  * \sa readTime() method
  *
  * \return no return value.
  */
  void now(void) { readTime(); }

 /**
  * Compatibility function - Check if RTC is running
  *
  * Wrapper for a HALT status check
  *
  * \sa status() method
  *
  * \return true if running, false otherwise.
  */
  boolean isRunning(void) { return(status(DS3231_CLOCK_HALT) != DS3231_ON); }

 /** @} */

 //--------------------------------------------------------------
 /** \name Methods for Alarm 1 operations
  * @{
  */
 /**
  * Read the current Alarm 1 time into the interface registers
  *
  * Query the RTC for the current alarm 1 trigger time and load it 
  * into the library interface registers (dd, h, m, s, dow, pm) from 
  * which the data can be accessed. 
  *
  * \sa readAlarm1() method
  *
  * \return false if errors, true otherwise.
  */
  boolean readAlarm1(void);

 /**
  * Write the current Alarm 1 time from the interface registers
  *
  * Write the data in the interface registers (dd, h, m, s, dow, pm) 
  * as the Alarm 1 trigger time in the RTC and set the alarm trigger to one of the values in 
  * almType_t, noting that not all values are valid for each alarm.
  *
  * \sa writeAlarm1() method
  *
  * \param   almType the type of alarm trigger required
  * \return false if errors, true otherwise.
  */
  boolean writeAlarm1(almType_t almType);

 /**
  * Set the Alarm 1 trigger type
  *
  * Set the alarm trigger to one of the values in almType_t, noting that not 
  * all values are valid for each alarm.
  *
  * \sa getAlarm1Type() method
  *
  * \param   almType the type of alarm trigger required
  * \return false if errors, true otherwise.
  */
  boolean setAlarm1Type(almType_t almType);

 /**
  * Get the Alarm 1 trigger type
  *
  * Read the alarm trigger from the RTC. This will be one of the values in almType_t, 
  * noting that not all values are valid for each alarm.
  *
  * \sa setAlarm1Type() method
  *
  * \return almType_t current setting for the alarm or ALM_ERROR if error
  */
  almType_t getAlarm1Type(void);

  /**
  * Check if Alarm 1 has triggered
  *
  * The method checks if the alarm triggered flag has been set. If it has
  * the callback function is invoked. In either case the triggered flag is reset.
  *
  * \sa setAlarm1Callback() method.
  * 
  * \return true if the alarm triggered, false otherwise.
  */
  boolean checkAlarm1(void);

 /**
  * Set the callback function for Alarm 1
  *
  * Pass the address of the callback function to the libraries. The callback function 
  * prototype is 
  * 
  * void functionName(void);
  *
  * and is invoked when the checkAlarm1() method is invoked. Set to NULL (default) 
  * to disable this feature.
  *
  * \sa setAlarm1Callback() method.
  * 
  * \param cb  the address of the callback function.
  * \return false if errors, true otherwise.
  */
  inline boolean setAlarm1Callback(void (*cb)(void)) { _cbAlarm1 = cb; };

 /** @} */

 //--------------------------------------------------------------
 /** \name Methods for Alarm 2 operations
  * @{
  */
 /**
  * Read the current Alarm 2 time into the interface registers
  *
  * Query the RTC for the current alarm 2 trigger time and load it
  * into the library interface registers (dd, h, m, s, dow, pm) from 
  * which the data can be accessed.
  *
  * \sa writeAlarm2() method
  *
  * \return false if errors, true otherwise.
  */
  boolean readAlarm2(void);  // read the alarm2 values

 /**
  * Write the current Alarm 2 time from the interface registers
  *
  * Write the data in the interface registers (dd, h, m, s, dow, pm) 
  * as the Alarm 2 trigger time in the RTC and set the alarm trigger to one of the values in 
  * almType_t, noting that not all values are valid for each alarm.
  *
  * \sa readAlarm2() method
  *
  * \param   almType the type of alarm trigger required
  * \return false if errors, true otherwise.
  */
  boolean writeAlarm2(almType_t almType); // write the alarm2 values and set almType

 /**
  * Set the Alarm 2 trigger type
  *
  * Set the alarm trigger to one of the values in almType_t, noting that not 
  * all values are valid for each alarm.
  *
  * \sa getAlarm2Type() method
  *
  * \param   almType the type of alarm trigger required
  * \return false if errors, true otherwise.
  */
  boolean setAlarm2Type(almType_t almType);

 /**
  * Get the Alarm 2 trigger type
  *
  * Read the alarm trigger from the RTC. This will be one of the values in almType_t, 
  * noting that not all values are valid for each alarm.
  *
  * \sa setAlarm2Type() method
  *
  * \return almType_t current setting for the alarm or ALM_ERROR if error
  */
  almType_t getAlarm2Type(void);

 /**
  * Check if Alarm 2 has triggered
  *
  * The method checks if the alarm triggered flag has been set. If it has
  * the callback function is invoked. In either case the triggered flag is reset. 
  *
  * \sa setAlarm2Callback() method.
  * 
  * \return true if the alarm triggered, false otherwise.
  */
  boolean checkAlarm2(void);

 /**
  * Set the callback function for Alarm 2
  *
  * Pass the address of the callback function to the libraries. The callback function 
  * prototype is 
  * 
  * void functionName(void);
  *
  * and is invoked when the checkAlarm2() method is invoked. Set to NULL (default) 
  * to disable this feature
  *
  * \sa setAlarm2Callback() method.
  * 
  * \param cb  the address of the callback function.
  * \return false if errors, true otherwise.
  */
  inline boolean setAlarm2Callback(void (*cb)(void)) { _cbAlarm2 = cb; };

 /** @} */

 //--------------------------------------------------------------
 /** \name Miscellaneous methods
  * @{
  */
 /**
  * Read the raw RTC clock data
  *
  * Read _len_ bytes from the RTC clock starting at _addr_ as raw data into the 
  * buffer supplied. The size of the buffer should be at least MAX_BUF bytes long 
  * (defined in the library cpp file).
  *
  * \sa writeRAM() method
  *
  * \param addr    starting address for the read.
  * \param buf      address of the receiving byte buffer.
  * \param len       number of bytes to read.
  * \return number of bytes successfully read.
  */
  uint8_t readRAM(uint8_t addr, uint8_t* buf, uint8_t len);

 /**
  * Write the raw RTC clock data
  *
  * Write _len_ bytes of data in the buffer supplied to the RTC clock starting at _addr_.
  * The size of the buffer should be at least _len_ bytes long.
  *
  * \sa readRAM() method
  *
  * \param addr    starting address for the write.
  * \param buf      address of the data buffer.
  * \param len       number of bytes to write.
  * \return number of bytes successfully written.
  */
  uint8_t writeRAM(uint8_t addr, uint8_t* buf, uint8_t len);

 /**
  * Calculate day of week for a given date
  *
  * Given the specified date, calculate the day of week.
  * 
  * \sa Wikipedia https://en.wikipedia.org/wiki/Determination_of_the_day_of_the_week
  *
  * \param yyyy  year for specified date. yyyy must be > 1752.
  * \param mm  month for the specified date where mm is in the range [1..12], 1 = January.
  * \param dd    date for the specified date in the range [1..31], where 1 = first day of the month.
  * \return dow value calculated [1..7], where 1 = Sunday.
  */
  uint8_t calcDoW(uint16_t yyyy, uint8_t mm, uint8_t dd);

#if ENABLE_TEMP_COMP
 /**
  * Read the temperature register in the RTC
  *
  * Read the temperature compensation register in the RTC in degrees C.
  * Resolution is to 0.25 degrees C on the fractional part.
  *
  * \return the temperature in degrees C.
  */
  float readTempRegister(void);
#endif
 /** @} */

 //--------------------------------------------------------------
 /** \name Public variables for reading and writing time data
  * @{
  */
  uint16_t yyyy;///< Year including the millennium and century. See setCentury() for more information.
  uint8_t mm;   ///< Month (1-12)
  uint8_t dd;   ///< Date of the month (1-31)
  uint8_t h;    ///< Hour of the day (1-12) or (0-23) depending on the am/pm or 24h mode setting
  uint8_t m;    ///< Minutes past the hour (0-59)
  uint8_t s;    ///< Seconds past the minute (0-59)
  uint8_t dow;  ///< Day of the week (1-7). Sequential number; day coding depends on the application and zero is an undefined value
  uint8_t pm;   ///< Non-zero if 12 hour clock mode and PM, always zero for 24 hour clock. Check the time and if < 12 then check this indicator.

  /** @} */

private:
  void (*_cbAlarm1)(void);
  void (*_cbAlarm2)(void);
  uint8_t _century;

  // BCD to binary number packing/unpacking functions
  inline uint8_t BCD2bin(uint8_t v) { return v - 6 * (v >> 4); }
  inline uint8_t bin2BCD (uint8_t v) { return v + 6 * (v / 10); }
  boolean unpackAlarm(uint8_t entryPoint);
  boolean packAlarm(uint8_t entryPoint);

  // Interface functions for the RTC device
  uint8_t readDevice(uint8_t addr, uint8_t* buf, uint8_t len);
  uint8_t writeDevice(uint8_t addr, uint8_t* buf, uint8_t len);
};

extern MD_DS3231 RTC;    ///< Library created instance of the RTC class

#endif
