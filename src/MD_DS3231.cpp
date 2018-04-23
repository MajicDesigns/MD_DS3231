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

class MD_DS3231 RTC;  // one instance created when library is included

// Useful definitions
#define DS3231_ID 0x68  // I2C/TWI device address, coded into the device

#define RAM_BASE_READ 0 // smallest read address

// Addresses for the parts of the date/time in RAM
#define ADDR_SEC  0x0
#define ADDR_MIN  0x1
#define ADDR_HR   0x2
#define ADDR_DAY  0x3
#define ADDR_TDATE 0x4
#define ADDR_ADATE 0x3
#define ADDR_MON  0x5
#define ADDR_YR   0x6

#define ADDR_CTL_12H  0x2
#define ADDR_CTL_PM   0x2
#define ADDR_CTL_DYDT 0x3
#define ADDR_CTL_100  0x5

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
// See the IC datasheet for the definitions of these bits
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

// Interface functions for the RTC device
uint8_t MD_DS3231::readDevice(uint8_t addr, uint8_t* buf, uint8_t len)
{
  Wire.beginTransmission(DS3231_ID);
  Wire.write(addr);       // set register address
  if (Wire.endTransmission() != 0)
    return(0);

  Wire.requestFrom(DS3231_ID, (int)len);
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
  for (uint8_t i=0; i<len; i++) // Send x data from given address upwards...
  {
    Wire.write(buf[i]);         // ... and send it from buffer
  }
  Wire.endTransmission();

  return(len);
}

// Class functions
MD_DS3231::MD_DS3231() : yyyy(0), mm(0), dd(0), h(0), m(0), s(0), dow(0),
_cbAlarm1(nullptr), _cbAlarm2(nullptr), _century(20)
{
  Wire.begin();
}
 
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
  uint8_t m1, m2, m3, m4, d; // A1M1, A1M2, A1M3, A1M4, DY/~DT

  switch (almType)  // set values from the datasheet
  {
    case DS3231_ALM_SEC:   d=0; m4=1; m3=1; m2=1; m1=1; break; // 01111
    case DS3231_ALM_S:     d=0; m4=1; m3=1; m2=1; m1=0; break; // 01110
    case DS3231_ALM_MS:    d=0; m4=1; m3=1; m2=0; m1=0; break; // 01100
    case DS3231_ALM_HMS:   d=0; m4=1; m3=0; m2=0; m1=0; break; // 01000
    case DS3231_ALM_DTHMS: d=0; m4=0; m3=0; m2=0; m1=0; break; // 00000
    case DS3231_ALM_DDHMS: d=1; m4=0; m3=0; m2=0; m1=0; break; // 10000
    default:  return(false);
  }

  // read the current data into the buffer
  readDevice(ADDR_ALM1, bufRTC, 4);

  // mask in the new data = clear the bit and then set current value
  bufRTC[0] = (bufRTC[0] & 0x7f) | (m1 << 7);
  bufRTC[1] = (bufRTC[1] & 0x7f) | (m2 << 7);
  bufRTC[2] = (bufRTC[2] & 0x7f) | (m3 << 7);
  bufRTC[3] = (bufRTC[3] & 0x3f) | (m4 << 7) | (d << 6);

  // write the data back out
  return(writeDevice(ADDR_ALM1, bufRTC, 4) == 4);
}

almType_t MD_DS3231::getAlarm1Type(void)
{
  uint8_t m = 0;

  // read the current data into the buffer
  readDevice(ADDR_ALM1, bufRTC, 4);

  // create a value with bit 0=M1, 1=M2, 2=M3, 3=M4
  m |= (bufRTC[0] & 0x80) >> 7;
  m |= (bufRTC[1] & 0x80) >> 6;
  m |= (bufRTC[2] & 0x80) >> 5;
  m |= (bufRTC[3] & 0x80) >> 4;

  switch (m)
  {
    case 0x0f: return(DS3231_ALM_SEC);  // 1111
    case 0x0e: return(DS3231_ALM_S);    // 1110
    case 0x0c: return(DS3231_ALM_MS);   // 1100
    case 0x08: return(DS3231_ALM_HMS);  // 1000
    case 0x00:
      if (bufRTC[3] & 0x40) // check the D bit
        return(DS3231_ALM_DDHMS);// 10000
      else
        return(DS3231_ALM_DTHMS);// 00000
  }    

  return(DS3231_ALM_ERROR);
}

boolean MD_DS3231::setAlarm2Type(almType_t almType)
{
  uint8_t m2, m3, m4, d; // A2M2, A2M3, A2M4, DY/~DT

  switch (almType)  // set values from the datasheet
  {
    case DS3231_ALM_MIN:   d=0; m4=1; m3=1; m2=1; break; // 0111
    case DS3231_ALM_M:     d=0; m4=1; m3=1; m2=0; break; // 0110
    case DS3231_ALM_HM:    d=0; m4=1; m3=0; m2=0; break; // 0100
    case DS3231_ALM_DTHM:  d=0; m4=0; m3=0; m2=0; break; // 0000
    case DS3231_ALM_DDHM:  d=1; m4=0; m3=0; m2=0; break; // 1000
    default:  return(false);
  }

  // read the current data into the buffer
  readDevice(ADDR_ALM2, bufRTC, 3);

  // mask in the new data = clear the bit and then set current value
  bufRTC[0] = (bufRTC[0] & 0x7f) | (m2 << 7);
  bufRTC[1] = (bufRTC[1] & 0x7f) | (m3 << 7);
  bufRTC[2] = (bufRTC[2] & 0x3f) | (m4 << 7) | (d << 6);

  // write the data back out
  return(writeDevice(ADDR_ALM2, bufRTC, 3) == 3);  
}

almType_t MD_DS3231::getAlarm2Type(void)
{
  uint8_t m = 0;

  // read the current data into the buffer
  readDevice(ADDR_ALM2, bufRTC, 3);

  // create a value with bit 0=M2, 2=M3, 3=M4
  m |= (bufRTC[0] & 0x80) >> 7;
  m |= (bufRTC[1] & 0x80) >> 6;
  m |= (bufRTC[2] & 0x80) >> 5;

  switch (m)
  {
    case 0x7: return(DS3231_ALM_MIN); // 111
    case 0x6: return(DS3231_ALM_M);   // 110
    case 0x4: return(DS3231_ALM_HM);  // 100
    case 0x0:
      if (bufRTC[2] & 0x40) // check the D bit
        return(DS3231_ALM_DDHM);// 1000
      else
        return(DS3231_ALM_DTHM);// 0000
  }

  return(DS3231_ALM_ERROR);
}

boolean MD_DS3231::unpackAlarm(uint8_t entryPoint)
// general routine for unpacking alarm registers from device
// Assumes the buffer is set up as per Alarm 1 registers. For Alarm 2 (missing seconds), 
// the first byte of the Alarm data should in byte 1
{
  switch(entryPoint)
  {
    case 1:   // Alarm 1 registers
      s = BCD2bin(bufRTC[ADDR_SEC]);
      // fall through
      
    case 2:   // Alarm 1 and Alarm 2 registers
      m = BCD2bin(bufRTC[ADDR_MIN]);
      if (bufRTC[ADDR_CTL_12H] & CTL_12H) 			// 12 hour clock
      {
        h = BCD2bin(bufRTC[ADDR_HR] & 0x1f);
        pm = (bufRTC[ADDR_CTL_PM] & CTL_PM);
      } 
      else
      {
        h = BCD2bin(bufRTC[ADDR_HR] & 0x3f);
        pm = 0;
      }
      
      if (bufRTC[ADDR_CTL_DYDT] & CTL_DYDT)   // Day or date?
      {
        dow = BCD2bin(bufRTC[ADDR_DAY] & 0x0f);
        dd = 0;
      } else {
        dd = BCD2bin(bufRTC[ADDR_ADATE] & 0x3f);
        dow = 0;
      }
  }

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
  readDevice(ADDR_TIME, bufRTC, 7);		// get the data

  // unpack it
  s = BCD2bin(bufRTC[ADDR_SEC]);
  m = BCD2bin(bufRTC[ADDR_MIN]);
  if (bufRTC[ADDR_CTL_12H] & CTL_12H) 			// 12 hour clock
  {
    h = BCD2bin(bufRTC[ADDR_HR] & 0x1f);
    pm = (bufRTC[ADDR_CTL_PM] & CTL_PM);
  }
  else
  {
    h = BCD2bin(bufRTC[ADDR_HR] & 0x3f);
    pm = 0;
  }
  dow = BCD2bin(bufRTC[ADDR_DAY]);
  dd = BCD2bin(bufRTC[ADDR_TDATE]);
  mm = BCD2bin(bufRTC[ADDR_MON]);
  yyyy = BCD2bin(bufRTC[ADDR_YR]) + (_century*100);
  if (bufRTC[ADDR_CTL_100] & CTL_100)
    yyyy += 100;

  return(true);
}

boolean MD_DS3231::packAlarm(uint8_t entryPoint)
{
    boolean mode12 = (status(DS3231_12H) == DS3231_ON);

    CLEAR_BUFFER;
    
    // check what time mode is current
    switch (entryPoint)
    {
      case 1:
        bufRTC[ADDR_SEC] = bin2BCD(s);
        // fall through
        
      case 2:
        bufRTC[ADDR_MIN] = bin2BCD(m);
        if (mode12)     // 12 hour clock
        {
          uint8_t	hour = bin2BCD(h);

          pm = (hour > 12);
          if (pm) hour -= 12;
          bufRTC[ADDR_HR] = bin2BCD(hour);
          if (pm) bufRTC[ADDR_CTL_PM] |= CTL_PM;
          bufRTC[ADDR_CTL_12H] |= CTL_12H;
        }
        else
          bufRTC[ADDR_HR] = bin2BCD(h);

        if (dow == 0) // signal that this is a date, not day
        {
          bufRTC[ADDR_DAY] = bin2BCD(dow);
          bufRTC[ADDR_CTL_DYDT] |= CTL_DYDT; 
        }
        else
        {
          bufRTC[ADDR_ADATE] = bin2BCD(dd);
          bufRTC[ADDR_CTL_DYDT] &= ~CTL_DYDT;
        }
    }
}

boolean MD_DS3231::writeAlarm1(almType_t almType)
{
  packAlarm(1);
  if (!writeDevice(ADDR_ALM1, bufRTC, 4) == 4)
    return(false);
  return(setAlarm1Type(almType));
}

boolean MD_DS3231::writeAlarm2(almType_t almType)
{
  packAlarm(2);
  if (!writeDevice(ADDR_ALM2, &bufRTC[1], 3) == 3)
    return(false);
  return(setAlarm2Type(almType));
}

boolean MD_DS3231::writeTime(void)
// Pack up and write the time stored in the object variables to the RTC
// Note: Setting the time will also start the clock of it is halted
// return true if the function succeeded
{
  boolean mode12 = (status(DS3231_12H) == DS3231_ON);
  
  CLEAR_BUFFER;
  
  // pack it up in the current space
  bufRTC[ADDR_SEC] = bin2BCD(s);
  bufRTC[ADDR_MIN] = bin2BCD(m);
  if (mode12)				// 12 hour clock
  {
    pm = (h > 12);
    if (pm) h -= 12;
    bufRTC[ADDR_HR] = bin2BCD(h);
    if (pm) bufRTC[ADDR_CTL_PM] |= CTL_PM;
    bufRTC[ADDR_CTL_12H] |= CTL_12H;
  }
  else
    bufRTC[ADDR_HR] = bin2BCD(h);
    
  bufRTC[ADDR_DAY] = bin2BCD(dow);
  bufRTC[ADDR_TDATE] = bin2BCD(dd);
  bufRTC[ADDR_MON] = bin2BCD(mm);
  {
    uint16_t c = (yyyy-2000)/100;  // number of centuries
    
    bufRTC[ADDR_YR] = bin2BCD(yyyy - (_century*100) - (100*c));
    if (c > 0) bufRTC[ADDR_CTL_100] |= CTL_100;
  }
  
  return(writeDevice(ADDR_TIME, bufRTC, 7) == 7);
}

uint8_t MD_DS3231::readRAM(uint8_t addr, uint8_t* buf, uint8_t len)
// Read len bytes from the RTC, starting at address addr, and put them in buf
// Reading includes all bytes at addresses RAM_BASE_READ to DS3231_RAM_MAX
{
  if ((NULL == buf) || (addr < RAM_BASE_READ) || (len == 0) ||(addr + len - 1 > DS3231_RAM_MAX))
    return(0);

  CLEAR_BUFFER;

  return(readDevice(addr, buf, len));		// read all the data once
}

uint8_t MD_DS3231::writeRAM(uint8_t addr, uint8_t* buf, uint8_t len)
// Write len bytes from buffer buf to the RTC, starting at address addr
// Writing includes all bytes at addresses RAM_BASE_READ to DS3231_RAM_MAX
{
  if ((NULL == buf) || (addr < RAM_BASE_READ) || (len == 0) || (addr + len - 1 >= DS3231_RAM_MAX))
  return(0);

  return(writeDevice(addr, buf, len));	// write all the data at once
}

uint8_t MD_DS3231::calcDoW(uint16_t yyyy, uint8_t mm, uint8_t dd) 
// https://en.wikipedia.org/wiki/Determination_of_the_day_of_the_week
// This algorithm good for dates  yyyy > 1752 and  1 <= mm <= 12
// Returns dow  01 - 07, 01 = Sunday
{
  static int t[] = {0, 3, 2, 5, 0, 3, 5, 1, 4, 6, 2, 4};
    
  yyyy -= mm < 3;
  return ((yyyy + yyyy/4 - yyyy/100 + yyyy/400 + t[mm-1] + dd) % 7) + 1;
}

#if ENABLE_TEMP_COMP
float MD_DS3231::readTempRegister()
{
  if (readDevice(ADDR_TEMP_REGISTER, bufRTC, 2) != 2)
    return(0.0);
    
  return(bufRTC[0] + ((bufRTC[1] >> 6) * 0.25));
}
#endif

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
      return(false);	// parameters were wrong - make no fuss and just go back
  }

  // now read the address from the RTC
  if (readDevice(addr, bufRTC, 1) != 1)
    return(false);

  // do any special processing here
  if (item == DS3231_12H)		// changing 12/24H clock - special handling of hours conversion
  {
    switch(value)
    {
      case DS3231_ON:	// change to 12H ...
        if (!(bufRTC[0] & CTL_12H))	// ... and not in 12H mode
        {
          uint8_t	hour = BCD2bin(bufRTC[0] & 0x3f);
          
          if (hour > 12)			// adjust the time, otherwise it looks the same as it does
          {
            bufRTC[0] = bin2BCD(hour - 12);
            bufRTC[0] |= CTL_PM;
          }
        }
      break;

      case DS3231_OFF:	// change to 24H ...
        if ((bufRTC[0] & CTL_12H) && (bufRTC[0] & CTL_PM)) 	// ... not in 24H mode and it is PM
        {
          uint8_t	hour = BCD2bin(bufRTC[0] & 0x1f);
          bufRTC[0] = bin2BCD(hour + 12);
        }
      break;
    }
  }

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
