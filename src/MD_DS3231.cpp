/*
  MD_DS3231 - Library for using a DS3231 Real Time Clock.
  
  Created by Marco Colli July 2015
  
  This library is free software; you can redistribute it and/or
  modify it under the terms of the GNU Lesser General Public
  License as published by the Free Software Foundation; either
  version 2.1 of the License, or (at your option) any later version.
 */
#include <Wire.h>
#include "MD_DS3231.h"

#if ENABLE_RTC_INSTANCE
class MD_DS3231 RTC;  // one instance created when library is included
#endif

// Useful definitions
#define DS3231_ID ((uint8_t)0x68)  // I2C/TWI device address, coded into the device

#define RAM_BASE_READ 0 // smallest read address

// Addresses for the parts of the date/time in RAM
#define ADDR_SEC    ((uint8_t)0x0)
#define ADDR_MIN    ((uint8_t)0x1)
#define ADDR_HR     ((uint8_t)0x2)
#define ADDR_DAY    ((uint8_t)0x3)
#define ADDR_TDATE  ((uint8_t)0x4)
#define ADDR_ADATE  ((uint8_t)0x3)
#define ADDR_MON    ((uint8_t)0x5)
#define ADDR_YR     ((uint8_t)0x6)

#define ADDR_CTL_12H  ((uint8_t)0x2)
#define ADDR_CTL_PM   ((uint8_t)0x2)
#define ADDR_CTL_DYDT ((uint8_t)0x3)
#define ADDR_CTL_100  ((uint8_t)0x5)

#define CTL_12H   0x40
#define CTL_PM    0x20
#define CTL_DYDT  0x40
#define CTL_100   0x80

// Address for the registers or section starts
#define ADDR_TIME   0x0
#define ADDR_ALM1   0x7
#define ADDR_ALM2   0xb

#define ADDR_CONTROL_REGISTER 0x0e
#define ADDR_STATUS_REGISTER  0x0f
#define ADDR_AGING_REGISTER   0x10
#define ADDR_TEMP_REGISTER    0x11

// Bit masks for the control/testable bits
// See the IC data sheet for the definitions of these bits
#define CTL_EOSC  0x80  // Enable Oscillator - bit 7 control register
#define CTL_BBSQWE 0x40 // Battery Backed Square Wave Enable - bit 6 control register
#define CTL_CONV  0x20  // Convert Temperature - bit 5 control register
#define CTL_RS    0x18  // Rate Select - bit 4 and 3 control register
#define CTL_INTCN 0x04  // Interrupt control - bit 2 control register
#define CTL_A2IE  0x02  // Alarm 2 Enable Interrupt - bit 1 control register
#define CTL_A1IE  0x01  // Alarm 1 Enable Interrupt - bit 0 control register

#define STS_OSF   0x80  // Oscillator Stop Flag - bit 7 status register
#define STS_EN32KHZ 0x08  // Enable 32kHz output - bit 3 status register
#define STS_BSY   0x04  //  TXCO Busy Flag - bit 2 status register
#define STS_A2F   0x02  // Alarm 2 Flag - bit 1 status register
#define STS_A1F   0x01  // Alarm 1 Flag - bit 0 status register

// Define a global buffer we can use in these functions
#define MAX_BUF   8     // time message is the biggest message we need to handle (7 bytes)
uint8_t bufRTC[MAX_BUF];
#define CLEAR_BUFFER  { memset(bufRTC, 0, sizeof(bufRTC)); }

#define DEFAULT_CENTURY 20 // Default century used to compute the yyyy interface register

#if ENABLE_CENTURY
#define CENTURY _century
#else
#define CENTURY DEFAULT_CENTURY
#endif


// Interface functions for the RTC device
uint8_t MD_DS3231::readDevice(uint8_t addr, uint8_t* buf, uint8_t len)
{
  Wire.beginTransmission(DS3231_ID);
  Wire.write(addr);       // set register address
  if (Wire.endTransmission() != 0)
    return(0);

  Wire.requestFrom(DS3231_ID, len);
  //while (!Wire.available()) ;   // wait
  for (uint8_t i=0; i<len; i++) // Read x data from given address upwards...
  {
    buf[i] = Wire.read();       // ... and store it in the buffer
  }

  return(len);
}

uint8_t MD_DS3231::writeDevice(uint8_t addr, uint8_t* buf, uint8_t len)
{
  Wire.beginTransmission(DS3231_ID);
  Wire.write(addr);       // set register address 
  Wire.write(buf, len);   // ... and send it from buffer                 
  Wire.endTransmission();

  return(len);
}

// Class functions
MD_DS3231::MD_DS3231() : yyyy(0), mm(0), dd(0), h(0), m(0), s(0), 
#if ENABLE_DOW
dow(0),
#endif
_cbAlarm1(nullptr), _cbAlarm2(nullptr),
#if ENABLE_DYNAMIC_CENTURY
_century(DEFAULT_CENTURY)
#endif
{
  Wire.begin();
}

#ifdef ESP8266
MD_DS3231::MD_DS3231(int sda, int scl) : yyyy(0), mm(0), dd(0), h(0), m(0), s(0), 
#if ENABLE_DOW
dow(0),
#endif
#if ENABLE_DYNAMIC_CENTURY
_century(DEFAULT_CENTURY),
#endif
_cbAlarm1(nullptr), _cbAlarm2(nullptr) 
{
  Wire.begin(sda, scl);
}
#endif

boolean MD_DS3231::checkAlarm1(void)
// Check the alarm. If time happened then call the callback function and reset the flag
{
  boolean b = (status(DS3231_A1_FLAG) == DS3231_ON);

  if (b)
  {
    control(DS3231_A1_FLAG, DS3231_OFF);
    if (_cbAlarm1 != nullptr) _cbAlarm1();
  }    

  return(b);
}

boolean MD_DS3231::checkAlarm2(void)
// Check the alarm. If time happened then call the callback function and reset the flag
{
  boolean b = (status(DS3231_A2_FLAG) == DS3231_ON);

  if (b)
  {
    control(DS3231_A2_FLAG, DS3231_OFF);
    if (_cbAlarm2 != NULL) _cbAlarm2();
  }

  return(b);
}

boolean MD_DS3231::setAlarm1Type(almType_t almType)
{
  // read the current data into the buffer
  readDevice(ADDR_ALM1, bufRTC, 4);

  int16_t alm1Type = static_cast<int16_t>(almType);
  // split each bit of almType to the seventh bit
  for(uint8_t i = 0; i < 4; i++) {
    bitWrite(bufRTC[i], 7, bitRead(alm1Type, i));
  }
  // set the D bit
  bitWrite(bufRTC[3], 6, bitRead(alm1Type, 4));

  // write the data back out
  return(writeDevice(ADDR_ALM1, bufRTC, 4) == 4);
}

almType_t MD_DS3231::getAlarm1Type(void)
{
  // read the current data into the buffer
  if(readDevice(ADDR_ALM1, bufRTC, 4) != 4) return DS3231_ALM_ERROR;

  // create a value with bits 0=M1, 1=M2, 2=M3, 3=M4, 4=D
  uint16_t m = 0;
  for(uint8_t i = 0; i < 4; i++) {
    m |= (bufRTC[i] & 0x80) >> (7 - i);  
  }
  m |= (bufRTC[3] & 0x40) >> 2;

  return static_cast<almType_t>(m);  
}

boolean MD_DS3231::setAlarm2Type(almType_t almType)
{
  // read the current data into the buffer
  readDevice(ADDR_ALM2, bufRTC, 3);

  int16_t alm2Type = static_cast<int16_t>(almType);
  // split each bit of almType to the seventh bit
  for(uint8_t i = 0; i < 3; i++) {
    bitWrite(bufRTC[i], 7, bitRead(alm2Type, i));
  }
  // set the D bit
  bitWrite(bufRTC[2], 6, bitRead(alm2Type, 3));

  // write the data back out
  return(writeDevice(ADDR_ALM2, bufRTC, 3) == 3);  
}

almType_t MD_DS3231::getAlarm2Type(void)
{
  // read the current data into the buffer
  if(readDevice(ADDR_ALM2, bufRTC, 3) != 3) return DS3231_ALM_ERROR;

  // create a value with bits 0=M2, 1=M3, 2=M4, 4=D
  uint8_t m = 0;
  for(uint8_t i = 0; i < 3; i++) {
    m |= (bufRTC[i] & 0x80) >> (7 - i);  
  }
  m |= (bufRTC[2] & 0x40) >> 3;

  return static_cast<almType_t>(m | 0x40); //alarm2 types have the sixth bit set
}

boolean MD_DS3231::unpackAlarm(uint8_t entryPoint)
// general routine for unpacking alarm registers from device
// Assumes the buffer is set up as per Alarm 1 registers. For Alarm 2 (missing seconds), 
// the first byte of the Alarm data should in byte 1
{
  if (entryPoint < 2) s = BCD2bin(bufRTC[ADDR_SEC]);
  
  m = BCD2bin(bufRTC[ADDR_MIN]);
#if ENABLE_12H
  if (bufRTC[ADDR_CTL_12H] & CTL_12H)     // 12 hour clock
  {
    h = BCD2bin(bufRTC[ADDR_HR] & 0x1f);
    pm = (bufRTC[ADDR_CTL_PM] & CTL_PM);
  } 
  else
  {
#endif
    h = BCD2bin(bufRTC[ADDR_HR] & 0x3f);
#if ENABLE_12H
    pm = 0;
  }
#endif

#if ENABLE_DOW
  if (bufRTC[ADDR_CTL_DYDT] & CTL_DYDT)   // Day or date?
  {
    dow = BCD2bin(bufRTC[ADDR_DAY] & 0x0f);
    dd = 0;
  } else {
#endif
    dd = BCD2bin(bufRTC[ADDR_ADATE] & 0x3f);
#if ENABLE_DOW
    dow = 0;
  }
#endif

  return(true);
}

boolean MD_DS3231::readAlarm1(void)
// Read the current time from the RTC and unpack it into the object variables
// return true if the function succeeded
{
  readDevice(ADDR_ALM1, bufRTC, 4);
  unpackAlarm(1);

  return(true);
}

boolean MD_DS3231::readAlarm2(void)
// Read the current time from the RTC and unpack it into the object variables
// return true if the function succeeded
{
  readDevice(ADDR_ALM2, &bufRTC[1], 3);
  unpackAlarm(2);

  return(true);
}

boolean MD_DS3231::readTime(void)
// Read the current time from the RTC and unpack it into the object variables
// return true if the function succeeded
{
  readDevice(ADDR_TIME, bufRTC, 7);   // get the data

  // unpack it
  s = BCD2bin(bufRTC[ADDR_SEC]);
  m = BCD2bin(bufRTC[ADDR_MIN]);
#if ENABLE_12H
  if (bufRTC[ADDR_CTL_12H] & CTL_12H) // 12 hour clock
  {
    h = BCD2bin(bufRTC[ADDR_HR] & 0x1f);
    pm = (bufRTC[ADDR_CTL_PM] & CTL_PM);
  }
  else
  {
#endif
    h = BCD2bin(bufRTC[ADDR_HR] & 0x3f);
#if ENABLE_12H
    pm = 0;
  }
#endif
#if ENABLE_DOW
  dow = BCD2bin(bufRTC[ADDR_DAY]);
#endif
  dd = BCD2bin(bufRTC[ADDR_TDATE]);
  mm = BCD2bin(bufRTC[ADDR_MON]);

  yyyy = BCD2bin(bufRTC[ADDR_YR]) + (CENTURY * 100);
  if (bufRTC[ADDR_CTL_100] & CTL_100)
    yyyy += 100;

  return(true);
}

boolean MD_DS3231::packAlarm(uint8_t entryPoint)
{
#if ENABLE_12H
    // check what time mode is current
    boolean mode12 = (status(DS3231_12H) == DS3231_ON);
#endif

    CLEAR_BUFFER;
    
    if (entryPoint < 2) bufRTC[ADDR_SEC] = bin2BCD(s);
    
    bufRTC[ADDR_MIN] = bin2BCD(m);
#if ENABLE_12H
    if (mode12)     // 12 hour clock
    {
      if (h > 12) {
        h -= 12;
        pm = true;
      }
          
      bufRTC[ADDR_HR] = bin2BCD(h);
      if (pm) bufRTC[ADDR_CTL_PM] |= CTL_PM;
      bufRTC[ADDR_CTL_12H] |= CTL_12H;
    }
    else
#endif        
      bufRTC[ADDR_HR] = bin2BCD(h);
#if ENABLE_DOW
    if (dow != 0) // signal that this is a date, not day
    {
      bufRTC[ADDR_DAY] = bin2BCD(dow);
      bufRTC[ADDR_CTL_DYDT] |= CTL_DYDT; 
    }
    else
    {
#endif          
      bufRTC[ADDR_ADATE] = bin2BCD(dd);
      bufRTC[ADDR_CTL_DYDT] &= ~CTL_DYDT;
#if ENABLE_DOW
    }
#endif

    return(true);
}

boolean MD_DS3231::writeAlarm1(almType_t almType)
{
  packAlarm(1);
  if (writeDevice(ADDR_ALM1, bufRTC, 4) != 4)
    return(false);
  return(setAlarm1Type(almType));
}

boolean MD_DS3231::writeAlarm2(almType_t almType)
{
  packAlarm(2);
  if (writeDevice(ADDR_ALM2, &bufRTC[1], 3) != 3)
    return(false);
  return(setAlarm2Type(almType));
}

boolean MD_DS3231::writeTime(void)
// Pack up and write the time stored in the object variables to the RTC
// Note: Setting the time will also start the clock of it is halted
// return true if the function succeeded
{
#if ENABLE_12H
  boolean mode12 = (status(DS3231_12H) == DS3231_ON);
#endif  
  CLEAR_BUFFER;
  
  // pack it up in the current space
  bufRTC[ADDR_SEC] = bin2BCD(s);
  bufRTC[ADDR_MIN] = bin2BCD(m);
#if ENABLE_12H
  if (mode12)     // 12 hour clock
  {
    if (h > 12) {
      h -= 12;
      pm = true;
    }
    
    bufRTC[ADDR_HR] = bin2BCD(h);
    if (pm) bufRTC[ADDR_CTL_PM] |= CTL_PM;
    bufRTC[ADDR_CTL_12H] |= CTL_12H;
  }
  else
#endif
    bufRTC[ADDR_HR] = bin2BCD(h);
    
#if ENABLE_DOW
  bufRTC[ADDR_DAY] = bin2BCD(dow);
#endif
  bufRTC[ADDR_TDATE] = bin2BCD(dd);
  bufRTC[ADDR_MON] = bin2BCD(mm);

  uint16_t y = yyyy - (CENTURY * 100);
  if (y > 100) {
    bufRTC[ADDR_CTL_100] |= CTL_100;
    y -= 100;
  }
  bufRTC[ADDR_YR] = bin2BCD(y);
  
  return(writeDevice(ADDR_TIME, bufRTC, 7) == 7);
}

uint8_t MD_DS3231::readRAM(uint8_t addr, uint8_t* buf, uint8_t len)
// Read len bytes from the RTC, starting at address addr, and put them in buf
// Reading includes all bytes at addresses RAM_BASE_READ to DS3231_RAM_MAX
{
  if ((NULL == buf) || (addr < RAM_BASE_READ) || 
      (len == 0) ||(addr + len - 1 > DS3231_RAM_MAX))
    return(0);

  CLEAR_BUFFER;

  return(readDevice(addr, buf, len));   // read all the data once
}

uint8_t MD_DS3231::writeRAM(uint8_t addr, uint8_t* buf, uint8_t len)
// Write len bytes from buffer buf to the RTC, starting at address addr
// Writing includes all bytes at addresses RAM_BASE_READ to DS3231_RAM_MAX
{
  if ((NULL == buf) || (addr < RAM_BASE_READ) || 
      (len == 0) || (addr + len - 1 >= DS3231_RAM_MAX))
  return(0);

  return(writeDevice(addr, buf, len));	// write all the data at once
}

uint8_t MD_DS3231::calcDoW(uint16_t yyyy, uint8_t mm, uint8_t dd) 
// https://en.wikipedia.org/wiki/Determination_of_the_day_of_the_week
// This algorithm good for dates  yyyy > 1752 and  1 <= mm <= 12
// Returns dow  01 - 07, 01 = Sunday
{
  static const uint8_t ts[] PROGMEM = {0, 3, 2, 5, 0, 3, 5, 1, 4, 6, 2, 4};
  uint8_t t;
  memcpy_P(&t, &ts[mm - 1], 1);
    
  yyyy -= mm < 3;
  return ((yyyy + yyyy/4 - yyyy/100 + yyyy/400 + t + dd) % 7) + 1;
}

float MD_DS3231::readTempRegister()
{
  if (readDevice(ADDR_TEMP_REGISTER, bufRTC, 2) != 2)
    return(0.0);
    
  return(bufRTC[0] + ((bufRTC[1] >> 6) * 0.25));
}

boolean MD_DS3231::control(codeRequest_t item, uint8_t value)
// Perform a control action on item, using the value
{
  uint8_t cmd = 0;    // value used to set new value (ORed to cleared bits in register)
  uint8_t	mask;       // mask used to clear the bits being set (NOT ANDed to clear the bits in the register)
  uint8_t addr = ADDR_CONTROL_REGISTER; // address of the byte to read, modify, write. Assume control register, change if not.

  switch (item)
  {
    case DS3231_CLOCK_HALT:
      mask = CTL_EOSC;
      switch (value)
      {
        case DS3231_ON:  cmd = mask; break;
        case DS3231_OFF: cmd = 0; break;
        default: return(false); // wrong - just go back
      }
    break;

    case DS3231_SQW_ENABLE:
      mask = CTL_BBSQWE;
      switch (value)
      {
        case DS3231_ON:  cmd = mask; break;
        case DS3231_OFF: cmd = 0; break;
        default: return(false); // wrong - just go back
      }
    break;

    case DS3231_SQW_TYPE:
      mask = CTL_RS;
      switch (value)
      {
        case DS3231_SQW_1HZ:  cmd = (0x00 << 3);	break;
        case DS3231_SQW_1KHZ: cmd = (0x01 << 3);	break;
        case DS3231_SQW_4KHZ: cmd = (0x02 << 3);	break;
        case DS3231_SQW_8KHZ: cmd = (0x03 << 3);	break;
        default: return(false); // wrong - just go back
      }
    break;

    case DS3231_12H:
      addr = ADDR_CTL_12H;
      mask = CTL_12H;
      switch (value)
      {
        case DS3231_ON:	 cmd = mask; break;
        case DS3231_OFF: cmd = 0; break;
        default: return(false); // wrong - just go back
      }
    break;

    case DS3231_TCONV:
      mask = CTL_CONV;
      switch (value)
      {
        case DS3231_ON:  cmd = mask; break;
        default: return(false); // wrong - just go back
      }
    break;

    case DS3231_INT_ENABLE:
      mask = CTL_INTCN;
      switch (value)
      {
        case DS3231_ON:  cmd = mask; break;
        case DS3231_OFF: cmd = 0; break;
        default: return(false); // wrong - just go back
      }
    break;

    case DS3231_A1_INT_ENABLE:
      mask = CTL_A1IE;
      switch (value)
      {
        case DS3231_ON:  cmd = mask; break;
        case DS3231_OFF: cmd = 0; break;
        default: return(false); // wrong - just go back
      }
    break;

    case DS3231_A2_INT_ENABLE:
      mask = CTL_A2IE;
      switch (value)
      {
        case DS3231_ON:  cmd = mask; break;
        case DS3231_OFF: cmd = 0; break;
        default: return(false); // wrong - just go back
      }
    break;
    
    case DS3231_A1_FLAG:
      addr = ADDR_STATUS_REGISTER;
      mask = STS_A1F;
      switch (value)
      {
        case DS3231_OFF: cmd = 0; break;
        default: return(false); // wrong - just go back
      }
    break;

    case DS3231_A2_FLAG:
      addr = ADDR_STATUS_REGISTER;
      mask = STS_A2F;
      switch (value)
      {
        case DS3231_OFF: cmd = 0; break;
        default: return(false); // wrong - just go back
      }
    break;

    case DS3231_32KHZ_ENABLE:
      addr = ADDR_STATUS_REGISTER;
      mask = STS_EN32KHZ;
      switch (value)
      {
        case DS3231_ON:  cmd = mask; break;
        case DS3231_OFF: cmd = 0; break;
        default: return(false); // wrong - just go back
      }
      break;

    case DS3231_HALTED_FLAG:
      addr = ADDR_STATUS_REGISTER;
      mask = STS_OSF;
      switch (value)
      {
        case DS3231_OFF: cmd = 0; break;
        default: return(false); // wrong - just go back
      }
      break;

    case DS3231_AGING_OFFSET:
      addr = ADDR_AGING_REGISTER;
      mask = 0xff;
      cmd = value;
      break;

    default:
      return(false);  // parameters were wrong - make no fuss and just go back
  }

  // now read the address from the RTC
  if (readDevice(addr, bufRTC, 1) != 1)
    return(false);

#if ENABLE_12H
  // do any special processing here
  if (item == DS3231_12H)   // changing 12/24H clock - special handling of hours conversion
  {
    switch(value)
    {
      case DS3231_ON: // change to 12H ...
        if (!(bufRTC[0] & CTL_12H)) // ... and not in 12H mode
        {
          uint8_t	hour = BCD2bin(bufRTC[0] & 0x3f);
          
          if (hour > 12)      // adjust the time, otherwise it looks the same as it does
          {
            bufRTC[0] = bin2BCD(hour - 12);
            bufRTC[0] |= CTL_PM;
          }
        }
      break;

      case DS3231_OFF:  // change to 24H ...
        if ((bufRTC[0] & CTL_12H) && (bufRTC[0] & CTL_PM))  // ... not in 24H mode and it is PM
        {
          uint8_t	hour = BCD2bin(bufRTC[0] & 0x1f);
          bufRTC[0] = bin2BCD(hour + 12);
        }
      break;
    }
  }
#endif

  // Mask off the new status, set the value and then write it back
  bufRTC[0] &= ~mask;
  bufRTC[0] |= cmd;
  return(writeDevice(addr, bufRTC, 1) == 1);
}

codeStatus_t MD_DS3231::status(codeRequest_t item)
// Obtain the status of the controllable item and return it.
// Return DS3231_ERROR otherwise.
{
  uint8_t	mask;       // mask used to clear the bits being set (NOT ANDed to clear the bits in the register)
  uint8_t addr = ADDR_STATUS_REGISTER;  // address of the byte to read, modify, write. Assume status register, change if not.

  switch (item)
  {
    case DS3231_HALTED_FLAG:  mask = STS_OSF;     break;
    case DS3231_A1_FLAG:      mask = STS_A1F;     break;
    case DS3231_A2_FLAG:      mask = STS_A2F;     break;
    case DS3231_32KHZ_ENABLE: mask = STS_EN32KHZ; break;
    case DS3231_BUSY_FLAG:    mask = STS_BSY;     break;
    case DS3231_CLOCK_HALT:   mask = CTL_EOSC;  addr = ADDR_CONTROL_REGISTER; break;
    case DS3231_SQW_ENABLE:   mask = CTL_BBSQWE;addr = ADDR_CONTROL_REGISTER; break;
    case DS3231_SQW_TYPE:     mask = CTL_RS;    addr = ADDR_CONTROL_REGISTER; break;
    case DS3231_12H:          mask = CTL_12H;   addr = ADDR_CTL_12H;          break;
    case DS3231_TCONV:        mask = CTL_CONV;  addr = ADDR_CONTROL_REGISTER; break;
    case DS3231_INT_ENABLE:   mask = CTL_INTCN; addr = ADDR_CONTROL_REGISTER; break;
    case DS3231_A1_INT_ENABLE:mask = CTL_A1IE;  addr = ADDR_CONTROL_REGISTER; break;
    case DS3231_A2_INT_ENABLE:mask = CTL_A2IE;  addr = ADDR_CONTROL_REGISTER; break;
    case DS3231_AGING_OFFSET: mask = 0xff;      addr = ADDR_AGING_REGISTER;   break;
    default:  return(DS3231_ERROR);   // invalid code request
  }

  // read the data and return appropriate value
  readDevice(addr, bufRTC, 1);
  
  // Handle any multi-bit values
  if (item == DS3231_SQW_TYPE)
  {
    switch((bufRTC[0] & mask) >> 3)
    {
        case 0: return(DS3231_SQW_1HZ);
        case 1: return(DS3231_SQW_1KHZ);
        case 2: return(DS3231_SQW_4KHZ);
        case 3: return(DS3231_SQW_8KHZ);
    }
  } 
  else if (item == DS3231_AGING_OFFSET)
  {
    return((codeStatus_t)bufRTC[0]);
  }
  
  // any other parameters are single bit ON of OFF
  return((bufRTC[0] & mask) ? DS3231_ON : DS3231_OFF);
}
