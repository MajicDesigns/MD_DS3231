## MD_DS3231
The DS3231 is a low-cost, extremely accurate I2C real time clock (RTC) with an 
integrated temperature compensated crystal oscillator (TCXO) and crystal. Microprocessor interface is exclusively through an I2C bidirectional bus.

The device 
* incorporates a battery input, and maintains accurate timekeeping when main power to the device is interrupted. 
* maintains seconds, minutes, hours, day, date, month, and year information. 
* automatically adjusts dates for months with fewer than 31 days, including corrections for leap year. 
* operates in either the 24-hour or 12-hour format with an AM/PM indicator.
* includes two programmable time-of day alarms.
* has and frequency programmable square-wave output.

This library features access to all on-chip features
* Read and write clock time registers
* Read and write alarm registers and controls
* Read/write clock and alarm registers as RAM
* Control of square wave generator (on/off & frequency)
* Control of clock features (Square Wave type and on/off, 12/24H, day of week, etc)

[Library Documentation](https://majicdesigns.github.io/MD_DS3231/)