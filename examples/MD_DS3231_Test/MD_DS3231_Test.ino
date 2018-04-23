// Example program for the MD_DS3231 library
//
// Test the library and control all aspects of the RTC chip functions
// using from the serial monitor.

#include <MD_DS3231.h>
#include <Wire.h>

#define PRINTS(s)   Serial.print(F(s))
#define PRINT(s, v) { Serial.print(F(s)); Serial.print(v); }

void setup()
{
  Serial.begin(57600);
  usage();
  RTC.setAlarm1Callback(cbAlarm1);
  RTC.setAlarm2Callback(cbAlarm2);
}

void usage(void)
{
  PRINTS("\n[MD_DS3231_Tester]");
  PRINTS("\n?\thelp - this message");
  PRINTS("\ntw yyyymmdd hhmmss dw\twrite the current date, time and day of week (1-7)");
  PRINTS("\ntr\tread the current time");
  PRINTS("\nrr\tread the IC registers as raw RAM");
  PRINTS("\ns\tstatus of the RTC");
  PRINTS("\nd\tcalculate day of week from current date");
  PRINTS("\n\na1i v\talm 1 interrupt enable (v 0=disabled, 1=enabled)");
  PRINTS("\na1f\talm 1 flag reset");
  PRINTS("\na1t v\talm 1 set type (v 0=all s, 1=s, 2=s+m, 3=s+m+h, 4=s+m+h+dt, 5=s+m+h+dy)");
  PRINTS("\na1z\talm 1 zero all values");
  PRINTS("\na1w t v\talm 1 write type t=(as above) time v=yyyymmdd hhmmss dw");
  PRINTS("\n\na2i v\talm2 interrupt enable (v 0=disabled, 1=enabled)");
  PRINTS("\na2f\talm 2 flag reset");
  PRINTS("\na2t v\talm2 set type (v 0=all m, 1=m, 2=m+h, 3=m+h+dt, 4=m+h+dy)");
  PRINTS("\na2z\talm 2 zero all values");
  PRINTS("\na2w t v\talm 2 write type t=(as above) time v=yyyymmdd hhmmss dw");
  PRINTS("\n\nc n v\tcontrol write value v to status n, where n is");
  PRINTS("\n\t0 - Clock Halt (v 0=run, 1=halt)");
  PRINTS("\n\t1 - BB SQW Enable(v 0=halt, 1=run)");
  PRINTS("\n\t2 - SQ Wave Freq (v 1=1Hz, 2=1khz, 3=4khz, 4=8kHz)");
  PRINTS("\n\t3 - TCXO conversion (v unused)");
  PRINTS("\n\t4 - 12 hour mode (v 0=24h, 1=12h)");
  PRINTS("\n\t5 - Alm interrupt operation (v 0=disabled, 1=enabled)");
  PRINTS("\n\t6 - Reset Halted Flag (v unused)");
  PRINTS("\n\t7 - Enabled 32kHz output (v 0=disabled, 1=enabled)");
  PRINTS("\n\t8 - Set aging register (v hex value)");
}

const char *dow2String(uint8_t code)
{
  static const char *str[] = {" ---", " Sun", " Mon", " Tue", " Wed", " Thu", " Fri", " Sat"};

  if (code > 7) code = 0;
  return(str[code]);
}

const char *ctl2String(codeRequest_t code)
{
 switch (code)
 {
  case DS3231_CLOCK_HALT: return("CLOCK HALT");
  case DS3231_SQW_ENABLE: return("SQW ENABLE");
  case DS3231_SQW_TYPE:   return("SQW TYPE");
  case DS3231_12H:        return("12H MODE");
  case DS3231_TCONV:      return("TEMP CONV");
  case DS3231_INT_ENABLE: return("INTERRUPT ENABLE");
  case DS3231_A1_INT_ENABLE:return("ALM1 ENABLE");
  case DS3231_A2_INT_ENABLE:return("ALM2 ENABLE");
  case DS3231_HALTED_FLAG:return("HALTED FLAG");
  case DS3231_32KHZ_ENABLE:return("32KHZ ENABLE");
  case DS3231_BUSY_FLAG:  return("BUSY FLAG");
  case DS3231_A1_FLAG:    return("ALM1 FLAG");
  case DS3231_A2_FLAG:    return("ALM2 FLAG");
  case DS3231_AGING_OFFSET:return("AGING OFFSET");
  default: return("??");
 };
}

const char *sts2String(codeStatus_t code, boolean bHexValue = false)
{
  if (!bHexValue)
  {
    switch (code)
    {
      case DS3231_ERROR:    return("ERROR");
      case DS3231_ON:       return("ON");
      case DS3231_OFF:      return("OFF");
      case DS3231_SQW_1HZ:  return("1Hz");
      case DS3231_SQW_1KHZ: return("1KHz");
      case DS3231_SQW_4KHZ: return("4KHz");
      case DS3231_SQW_8KHZ: return("8KHz");
      default: return("??");
    }
  }          
  else
  {
    static char sz[5];
  
    sprintf(sz, "0x%02x", code);
    return(sz);    
  }
}

const char *alm2String(almType_t alm)
{
  switch (alm)
  {
    case DS3231_ALM_ERROR:return("ALM_ERROR");
    case DS3231_ALM_SEC:  return("ALM_SEC");
    case DS3231_ALM_S:    return("ALM_S");
    case DS3231_ALM_MIN:  return("ALM_MIN");
    case DS3231_ALM_M:    return("ALM_M");
    case DS3231_ALM_MS:   return("ALM_MS");
    case DS3231_ALM_HM:   return("ALM_HM");
    case DS3231_ALM_HMS:  return("ALM_HMS");
    case DS3231_ALM_DTHM: return("ALM_DTHM");
    case DS3231_ALM_DTHMS:return("ALM_DTHMS");
    case DS3231_ALM_DDHM: return("ALM_DDHM");
    case DS3231_ALM_DDHMS:return("ALM_DDHMS");
    default: return("??");
  };
}

uint8_t htoi(char c)
{
  c = toupper(c);
  
  if (c >= '0' && c <= '9')
      return(c - '0');
  else if (c >= 'A' && c <= 'F')
      return(c - 'A' + 10);
  else
      return(0);
}
        
uint8_t i2dig(uint8_t mode)
// input 2 digits in the specified base
{
  uint8_t  v = 0;
  char    c[3] = "00";

  c[0] = readNext();
  c[1] = readNext();

  switch (mode)
  {
    case DEC: v = atoi(c);  break;
    case HEX: v = (htoi(c[0]) << 4) + htoi(c[1]); ;  break;
  }
  
  return(v);
}

char *p2dig(uint8_t v, uint8_t mode)
// 2 digits leading zero
{
  static char s[3];
  uint8_t i = 0;
  uint8_t n = 0;
 
  switch(mode)
  {
    case HEX: n = 16;  break;
    case DEC: n = 10;  break;
  }

  if (v < n) s[i++] = '0';
  itoa(v, &s[i], n);
  
  return(s);
}

void showStatus()
{
  PRINTS("\n>- Settings -<");
  PRINT("\n/EOSC Halt Enabled:\t", sts2String(RTC.status(DS3231_CLOCK_HALT)));
  PRINT("\nBBSQW Sq Wave Enable:\t", sts2String(RTC.status(DS3231_SQW_ENABLE)));
  PRINT("\nRS1_2 Sq Wave Freq:\t", sts2String(RTC.status(DS3231_SQW_TYPE)));
  PRINT("\nCONV  Temp Convert:\t", sts2String(RTC.status(DS3231_TCONV)));
  PRINT("\nINTCN Int enable:\t", sts2String(RTC.status(DS3231_INT_ENABLE)));
  PRINT("\n      12h mode:\t\t", sts2String(RTC.status(DS3231_12H)));
  PRINT("\nEN32k 32kHz enabled:\t", sts2String(RTC.status(DS3231_32KHZ_ENABLE)));
  PRINT("\nAGING Aging Offset:\t", sts2String(RTC.status(DS3231_AGING_OFFSET), true));
  
  PRINTS("\n\n>- Status -<");
  PRINT("\nBSY  Busy Flag:\t\t", sts2String(RTC.status(DS3231_BUSY_FLAG)));
  PRINT("\nOSF  Halted Flag:\t", sts2String(RTC.status(DS3231_HALTED_FLAG)));
  #if ENABLE_TEMP_COMP
  PRINT("\nTEMP Temp register:\t", RTC.readTempRegister());
  #endif

  PRINTS("\n\n>- Alarm 1 at ");
  showAlarm1();
  PRINT("\nA1IE   Intrpt:\t", sts2String(RTC.status(DS3231_A1_INT_ENABLE)));
  PRINT("\nA1F    Flag:\t", sts2String(RTC.status(DS3231_A1_FLAG)));
  PRINT("\nA1M1_4 Type:\t", alm2String(RTC.getAlarm1Type()));

  PRINTS("\n\n>- Alarm 2 at ");
  showAlarm2();
  PRINT("\nA2IE   Intrpt:\t", sts2String(RTC.status(DS3231_A2_INT_ENABLE)));
  PRINT("\nA2F    Flag:\t", sts2String(RTC.status(DS3231_A2_FLAG)));
  PRINT("\nA2M2_4 Type:\t", alm2String(RTC.getAlarm2Type()));
}

void printTime()
{
  PRINT("", RTC.yyyy);
  PRINT("-", p2dig(RTC.mm, DEC));
  PRINT("-", p2dig(RTC.dd, DEC));
  PRINT(" ", p2dig(RTC.h, DEC));
  PRINT(":", p2dig(RTC.m, DEC));
  PRINT(":", p2dig(RTC.s, DEC));
  if (RTC.status(DS3231_12H) == DS3231_ON)
    PRINT("", RTC.pm ? " pm" : " am");
  PRINT("", dow2String(RTC.dow));
}

void showTime()
{
  RTC.readTime();
  PRINTS("\nTime - ");
  printTime(); 
}

void showAlarm1()
{
  RTC.readAlarm1();
  printTime();
}

void showAlarm2()
{
  RTC.readAlarm2();
  printTime();
}

void showRAM()
{
  #define  MAX_READ_BUF  (DS3231_RAM_MAX / 8)  // do 8 lines
 
  uint8_t  k, regCount = 0;
  uint8_t  buf[MAX_READ_BUF];
  
  PRINTS("\n\n--------");
  for (uint8_t i=0; i<DS3231_RAM_MAX; i+=MAX_READ_BUF)
  {
    RTC.readRAM(i, buf, MAX_READ_BUF);
    
    PRINT("\n", p2dig(i, HEX));
    PRINTS(": ");
    for (uint8_t j=0; j<MAX_READ_BUF; j++)
    {
      if (regCount < DS3231_RAM_MAX)
      {
        PRINT("", p2dig(buf[j], HEX));
        PRINTS(" ");
        k = j;  // save this index for the next loop
        regCount++;
      } else
        PRINTS("   ");
    } 
    PRINTS("  ");
    for (uint8_t j=0; j<=k; j++)
    {
      if (isalnum(buf[j]) || ispunct(buf[j]))
      {
        PRINT("", (char) buf[j]);
      } else {
        PRINTS(".");
      }      
      PRINTS(" ");
    } 
  }
  PRINTS("\n--------");
}

void inputTime(void)
{
  RTC.yyyy = i2dig(DEC)*100 + i2dig(DEC);
  RTC.mm = i2dig(DEC);
  RTC.dd = i2dig(DEC);
  
  RTC.h = i2dig(DEC);
  RTC.m = i2dig(DEC);
  RTC.s = i2dig(DEC);
  
  RTC.dow = i2dig(DEC);
}  
  
void showDoW(void)
{
  RTC.readTime();
  PRINT("\nCalculated DoW (", RTC.calcDoW(RTC.yyyy, RTC.mm, RTC.dd));
  PRINT(") is ", dow2String(RTC.calcDoW(RTC.yyyy, RTC.mm, RTC.dd)));
}

void cbAlarm1()
// callback function for Alarm 2
{
  showTime();
  PRINTS("\t-> Alarm 1");  
}

void cbAlarm2()
// callback function for Alarm 2
{
  showTime();
  PRINTS("\t-> Alarm 2");
}

void doAlarm1()
{
  char  c = readNext();
  codeRequest_t item;
  codeStatus_t  value;
  almType_t alm;
  
  switch (toupper(c))
  {
    case 'I': // interrupt enable
      item = DS3231_A1_INT_ENABLE;
      c = readNext();
      switch (toupper(c))
      {
        case '0': value = DS3231_OFF;  break;
        case '1': value = DS3231_ON;  break;
        default: goto error;
      }

      PRINT("\nAlarm 1 ", ctl2String(item));
      PRINT(" value ", sts2String(value));
      PRINT(" result ", RTC.control(item, value));
      break;
      
    case 'F': // alarm flag
      item = DS3231_A1_FLAG;
      value = DS3231_OFF;
      PRINT("\nAlarm 1 ", ctl2String(item));
      PRINT(" value ", sts2String(value));
      PRINT(" result ", RTC.control(item, value));
      break;
      
    case 'T': // alarm type
      c = readNext();
      switch (toupper(c))
      {
        case '0': alm = DS3231_ALM_SEC;   break;
        case '1': alm = DS3231_ALM_S;     break;
        case '2': alm = DS3231_ALM_MS;    break;
        case '3': alm = DS3231_ALM_HMS;   break;
        case '4': alm = DS3231_ALM_DTHMS; break;
        case '5': alm = DS3231_ALM_DDHMS; break;
        default: goto error;
      }
      PRINT("\nAlarm 1 set type ", alm2String(alm));
      PRINT(" result ", RTC.setAlarm1Type(alm));
      break;

    case 'Z': // zero all values
      RTC.yyyy = RTC.mm = RTC.dd = 0;
      RTC.h = RTC.m = RTC.s = 0;
      RTC.dow = RTC.pm = 0;
      RTC.writeAlarm1(DS3231_ALM_DTHMS);
      showAlarm1();
      break;
      
    case 'W': // write alarm setting
      c = readNext();
      switch (toupper(c))
      {
        case '0': alm = DS3231_ALM_SEC;   break;
        case '1': alm = DS3231_ALM_S;     break;
        case '2': alm = DS3231_ALM_MS;    break;
        case '3': alm = DS3231_ALM_HMS;   break;
        case '4': alm = DS3231_ALM_DTHMS; break;
        case '5': alm = DS3231_ALM_DDHMS; break;
        default: goto error;
      }
      inputTime();
      RTC.writeAlarm1(alm);
      showAlarm1();
      break;
      
    default:
error:
    PRINTS("\nBad control element or parameter");
    return;
  }

  return;
}

void doAlarm2()
{
  char  c = readNext();
  codeRequest_t item;
  codeStatus_t  value;
  almType_t alm;
  
  switch (toupper(c))
  {
    case 'I': // interrupt enable
      item = DS3231_A2_INT_ENABLE;
      c = readNext();
      switch (toupper(c))
      {
        case '0': value = DS3231_OFF;  break;
        case '1': value = DS3231_ON;  break;
        default: goto error;
      }
      PRINT("\nAlarm 2 ", ctl2String(item));
      PRINT(" value ", sts2String(value));
      PRINT(" result ", RTC.control(item, value));
      break;
    
    case 'F': // alarm flag
      item = DS3231_A2_FLAG;
      value = DS3231_OFF;
      PRINT("\nAlarm 2 ", ctl2String(item));
      PRINT(" value ", sts2String(value));
      PRINT(" result ", RTC.control(item, value));
      break;
    
    case 'T': // alarm type
      c = readNext();
      switch (toupper(c))
      {
        case '0': alm = DS3231_ALM_MIN;   break;
        case '1': alm = DS3231_ALM_M;     break;
        case '2': alm = DS3231_ALM_HM;    break;
        case '3': alm = DS3231_ALM_DTHM;  break;
        case '4': alm = DS3231_ALM_DDHM;  break;
        default: goto error;
      }
      PRINT("\nAlarm 2 set type ", alm2String(alm));
      PRINT(" result ", RTC.setAlarm2Type(alm));
      break;

    case 'Z': // zero all values
      RTC.yyyy = RTC.mm = RTC.dd = 0;
      RTC.h = RTC.m = RTC.s = 0;
      RTC.dow = RTC.pm = 0;
      RTC.writeAlarm2(DS3231_ALM_DTHM);
      showAlarm2();
      break;

    case 'W': // write alarm setting
      c = readNext();
      switch (toupper(c))
      {
        case '0': alm = DS3231_ALM_MIN;   break;
        case '1': alm = DS3231_ALM_M;     break;
        case '2': alm = DS3231_ALM_HM;    break;
        case '3': alm = DS3231_ALM_DTHM;  break;
        case '4': alm = DS3231_ALM_DDHM;  break;
        default: goto error;
      }
      inputTime();
      RTC.writeAlarm2(alm);
      showAlarm2();
      break;
    
    default:
error:
      PRINTS("\nBad control element or parameter");
      return;
  }

  return;
}

void writeControl()
{
  char  c = readNext();
  codeRequest_t item;
  codeStatus_t  value;
  
  switch (toupper(c))
  {
    case '0':  // halt
      item = DS3231_CLOCK_HALT;
      c = readNext();
      switch (toupper(c))
      {
        case '0': value = DS3231_OFF;  break;
        case '1': value = DS3231_ON;  break;
        default: goto error;
      }
      break;
      
    case '1':  // enable
      item = DS3231_SQW_ENABLE;
      c = readNext();
      switch (toupper(c))
      {
        case '0': value = DS3231_OFF;  break;
        case '1': value = DS3231_ON;   break;
        default: goto error;
      }
      break;
      
    case '2':  // type on
      item = DS3231_SQW_TYPE;
      c = readNext();
      switch (toupper(c))
      {
        case '1': value = DS3231_SQW_1HZ;   break;
        case '2': value = DS3231_SQW_1KHZ;  break;
        case '3': value = DS3231_SQW_4KHZ;  break;
        case '4': value = DS3231_SQW_8KHZ;  break;
        default: goto error;
      }
      break;
      
    case '3':  // tcxo conversion
      item = DS3231_TCONV;
      value = DS3231_ON;
      break;
      
    case '4':  // 12 h mode
      item = DS3231_12H;
      c = readNext();
      switch (toupper(c))
      {
        case '0': value = DS3231_OFF;  break;
        case '1': value = DS3231_ON;   break;
        default: goto error;
      }
      break;
      
    case '5':  // alm interrupt operation
      item = DS3231_INT_ENABLE;
      c = readNext();
      switch (toupper(c))
      {
        case '0': value = DS3231_OFF;  break;
        case '1': value = DS3231_ON;   break;
        default: goto error;
      }
      break;

    case '6':  // reset halted flag
      item = DS3231_HALTED_FLAG;
      value = DS3231_OFF;
      break;

    case '7':  // enable 32kHz output
      item = DS3231_32KHZ_ENABLE;
      c = readNext();
      switch (toupper(c))
      {
        case '0': value = DS3231_OFF;  break;
        case '1': value = DS3231_ON;   break;
        default: goto error;
      }
      break;

    case '8':  // enable 32kHz output
      item = DS3231_AGING_OFFSET;
      value = (codeStatus_t) i2dig(HEX);
      break;

    default:
 error:
      PRINTS("\nBad control element or parameter");
      return;
  }
  
  // do it
  PRINT("\nControlling ", ctl2String(item));
  PRINT(" to ", sts2String(value, (item == DS3231_AGING_OFFSET)));
  PRINT(", result ", RTC.control(item, value));
  
  return;
}

char readNext()
// Read the next character from the serial input stream, skip whitespace.
// Busy loop that also checks for an alarm occurence.
{
  char  c;
  
  do
  {
    while (!Serial.available())
    {
      RTC.checkAlarm1();
      RTC.checkAlarm2();
    }      
    c = Serial.read();
  } while (isspace(c));

  return(c);
}

void loop()
{
   char  c;
   
   PRINTS("\n");
   // we need to get the next character to know what command we want to process 
   c = readNext();
   switch (toupper(c))
   {
     case '?':  usage();  break;
     
     case 'S':  showStatus();    break;
     case 'C':  writeControl();  break;
     case 'D':  showDoW();       break;
     case 'A':  
       c = readNext();
       switch(toupper(c))
       {
         case '1': doAlarm1();    break;
         case '2': doAlarm2();    break;
         default: goto no_good;
       }
       break;         
     
     // Read functions
     case 'R':  // Display updates      
       c = readNext();
       switch (toupper(c))
       {
         case 'R': showRAM();   break;
         default: goto no_good;
       }
       break;
    
     // Write functions
     case 'T':  // Display updates      
       c = readNext();
       switch (toupper(c))
       {
         case 'R': 
            showTime();  
            break;
            
         case 'W': 
            inputTime();
            RTC.writeTime();
            showTime();
            break;
            
         default: goto no_good;
       }
       break;

       default:  // don't know what to do with this! 
no_good:         // label for default escape when we can't process a character 
        {
          PRINT("\nBad parameter '", c);
          PRINTS("'");
          while (Serial.available())    // flush the buffer
            c = readNext();
        }
        break;
   }  
 }

