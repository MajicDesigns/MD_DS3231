// Example program for the MD_DS3231 library
//
// Test the library and control all aspects of the RTC chip functions
// using from the serial monitor.
//
// Dependencies:
// MD_cmdProcessor library available from https://github.com/MajicDesigns/MD_DS3231 or Arduino IDE Library Manager
//

#include <Wire.h>
#include <MD_DS3231.h>
#include <MD_cmdProcessor.h>

#define PRINTS(s)   Serial.print(F(s))
#define PRINT(s, v) { Serial.print(F(s)); Serial.print(v); }

#define ARRAY_SIZE(a)  (sizeof(a)/sizeof((a)[0]))

// handler function prototypes
void handlerHelp(char* param);

const char *dow2String(uint8_t code)
{
  static const char *str[] = {" ---", " Sun", " Mon", " Tue", " Wed", " Thu", " Fri", " Sat" };

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
        
char* i2dig(char *cp, uint16_t& v, uint8_t len, uint8_t base)
// input len digits in the specified base
{
  v = 0;

  for (uint8_t i = 0; i < len; i++)
  {
    switch (base)
    {
    case DEC: v = (v * 10) + (*cp - '0'); break;
    case HEX: v = (v << 4) + htoi(*cp);   break;
    }
    cp++;
  }
  
  return(cp);
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

void printTime(void)
{
  PRINT("\n", RTC.yyyy);
  PRINT("-", p2dig(RTC.mm, DEC));
  PRINT("-", p2dig(RTC.dd, DEC));
  PRINT(" ", p2dig(RTC.h, DEC));
  PRINT(":", p2dig(RTC.m, DEC));
  PRINT(":", p2dig(RTC.s, DEC));
  if (RTC.status(DS3231_12H) == DS3231_ON)
    PRINT("", RTC.pm ? " pm" : " am");
  PRINT("", dow2String(RTC.dow));
}

void inputTime(char* cp)
{
  uint16_t v;

  cp = i2dig(cp, v, 4, DEC);
  RTC.yyyy = v;
  cp = i2dig(cp, v, 2, DEC);
  RTC.mm = v;
  cp = i2dig(cp, v, 2, DEC);
  RTC.dd = v;

  cp = i2dig(cp, v, 2, DEC);
  RTC.h = v;
  cp = i2dig(cp, v, 2, DEC);
  RTC.m = v;
  cp = i2dig(cp, v, 2, DEC);
  RTC.s = v;

  cp = i2dig(cp, v, 1, DEC);
  RTC.dow = v;
}

void showTime(void)
{
  RTC.readTime();
  printTime(); 
}

void showAlarm1(void)
{
  RTC.readAlarm1();
  printTime();
}

void showAlarm2(void)
{
  RTC.readAlarm2();
  printTime();
}

void cbAlarm1(void)
// callback function for Alarm 2
{
  showTime();
  PRINTS("\t-> Alarm 1");
}

void cbAlarm2(void)
// callback function for Alarm 2
{
  showTime();
  PRINTS("\t-> Alarm 2");
}
  
// handler functions
void handlerTW(char* param)
{
  inputTime(param);
  RTC.writeTime();
  showTime();
}

void handlerTR(char* param)
{
  showTime();
}

void hanlderRR(char* param)
{
#define  MAX_READ_BUF  (DS3231_RAM_MAX / 8)  // do 8 lines

  uint8_t  k, regCount = 0;
  uint8_t  buf[MAX_READ_BUF];

  PRINTS("\n\n--------");
  for (uint8_t i = 0; i < DS3231_RAM_MAX; i += MAX_READ_BUF)
  {
    RTC.readRAM(i, buf, MAX_READ_BUF);

    PRINT("\n", p2dig(i, HEX));
    PRINTS(": ");
    for (uint8_t j = 0; j < MAX_READ_BUF; j++)
    {
      if (regCount < DS3231_RAM_MAX)
      {
        PRINT("", p2dig(buf[j], HEX));
        PRINTS(" ");
        k = j;  // save this index for the next loop
        regCount++;
      }
      else
        PRINTS("   ");
    }
    PRINTS("  ");
    for (uint8_t j = 0; j <= k; j++)
    {
      if (isalnum(buf[j]) || ispunct(buf[j]))
      {
        PRINT("", (char)buf[j]);
      }
      else {
        PRINTS(".");
      }
      PRINTS(" ");
    }
  }
  PRINTS("\n--------");
}

void handlerS(char* param)
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

void handlerD(char* param)
{
  RTC.readTime();
  PRINT("\nCalculated DoW (", RTC.calcDoW(RTC.yyyy, RTC.mm, RTC.dd));
  PRINT(") is", dow2String(RTC.calcDoW(RTC.yyyy, RTC.mm, RTC.dd)));
}

void handlerA1I(char* param)
{
  codeRequest_t item = DS3231_A1_INT_ENABLE;
  codeStatus_t  value;

  switch (toupper(*param))
  {
    case '0': value = DS3231_OFF;  break;
    case '1': value = DS3231_ON;  break;
    default:
      PRINT("\nBad parameter - ", *param);
      return;
  }

  PRINT("\nAlarm 1 ", ctl2String(item));
  PRINT(" value ", sts2String(value));
  PRINT(" result ", RTC.control(item, value));
}

void handlerA1F(char* param)
{
  codeRequest_t item = DS3231_A1_FLAG;
  codeStatus_t  value = DS3231_OFF;

  PRINT("\nAlarm 1 ", ctl2String(item));
  PRINT(" value ", sts2String(value));
  PRINT(" result ", RTC.control(item, value));
}

void handlerA1T(char* param)
{
  almType_t alm;

  switch (toupper(*param))
  {
    case '0': alm = DS3231_ALM_SEC;   break;
    case '1': alm = DS3231_ALM_S;     break;
    case '2': alm = DS3231_ALM_MS;    break;
    case '3': alm = DS3231_ALM_HMS;   break;
    case '4': alm = DS3231_ALM_DTHMS; break;
    case '5': alm = DS3231_ALM_DDHMS; break;
    default:
      PRINT("\nBad parameter - ", *param);
      return;
  }
  PRINT("\nAlarm 1 set type ", alm2String(alm));
  PRINT(" result ", RTC.setAlarm1Type(alm));
}

void handlerA1Z(char* param)
{
  RTC.yyyy = RTC.mm = RTC.dd = 0;
  RTC.h = RTC.m = RTC.s = 0;
  RTC.dow = RTC.pm = 0;
  RTC.writeAlarm1(DS3231_ALM_DTHMS);
  showAlarm1();
}

void handlerA1W(char* param)
{
  almType_t alm;

  switch (toupper(*param))
  {
    case '0': alm = DS3231_ALM_SEC;   break;
    case '1': alm = DS3231_ALM_S;     break;
    case '2': alm = DS3231_ALM_MS;    break;
    case '3': alm = DS3231_ALM_HMS;   break;
    case '4': alm = DS3231_ALM_DTHMS; break;
    case '5': alm = DS3231_ALM_DDHMS; break;
      PRINT("\nBad parameter - ", *param);
      return;
  }
  inputTime(param);
  RTC.writeAlarm1(alm);
  showAlarm1();
}

void handlerA2I(char* param)
{
  codeRequest_t item = DS3231_A2_INT_ENABLE;
  codeStatus_t  value;

  switch (toupper(*param))
  {
    case '0': value = DS3231_OFF;  break;
    case '1': value = DS3231_ON;  break;
    default:
      PRINT("\nBad parameter - ", *param);
      return;
  }

  PRINT("\nAlarm 2 ", ctl2String(item));
  PRINT(" value ", sts2String(value));
  PRINT(" result ", RTC.control(item, value));
}

void handlerA2F(char* param)
{
  codeRequest_t item = DS3231_A2_FLAG;
  codeStatus_t  value = DS3231_OFF;

  PRINT("\nAlarm 2 ", ctl2String(item));
  PRINT(" value ", sts2String(value));
  PRINT(" result ", RTC.control(item, value));
}

void handlerA2T(char* param)
{
  almType_t alm;

  switch (toupper(*param))
  {
    case '0': alm = DS3231_ALM_SEC;   break;
    case '1': alm = DS3231_ALM_S;     break;
    case '2': alm = DS3231_ALM_MS;    break;
    case '3': alm = DS3231_ALM_HMS;   break;
    case '4': alm = DS3231_ALM_DTHMS; break;
    case '5': alm = DS3231_ALM_DDHMS; break;
    default:
      PRINT("\nBad parameter - ", *param);
      return;
  }
  PRINT("\nAlarm 2 set type ", alm2String(alm));
  PRINT(" result ", RTC.setAlarm2Type(alm));
}

void handlerA2Z(char* param)
{
  RTC.yyyy = RTC.mm = RTC.dd = 0;
  RTC.h = RTC.m = RTC.s = 0;
  RTC.dow = RTC.pm = 0;
  RTC.writeAlarm2(DS3231_ALM_DTHMS);
  showAlarm2();
}

void handlerA2W(char* param)
{
  almType_t alm;

  switch (toupper(*param))
  {
    case '0': alm = DS3231_ALM_SEC;   break;
    case '1': alm = DS3231_ALM_S;     break;
    case '2': alm = DS3231_ALM_MS;    break;
    case '3': alm = DS3231_ALM_HMS;   break;
    case '4': alm = DS3231_ALM_DTHMS; break;
    case '5': alm = DS3231_ALM_DDHMS; break;
      PRINT("\nBad parameter - ", *param);
      return;
  }
  inputTime(param);
  RTC.writeAlarm2(alm);
  showAlarm2();
}

void handlerC(char* param)
{
  codeRequest_t item;
  codeStatus_t  value;

  switch (toupper(*param++))
  {
  case '0':  // halt
    item = DS3231_CLOCK_HALT;
    switch (toupper(*param))
    {
      case '0': value = DS3231_OFF;  break;
      case '1': value = DS3231_ON;  break;
      default: goto error;
    }
    break;

  case '1':  // enable
    item = DS3231_SQW_ENABLE;
    switch (toupper(*param))
    {
      case '0': value = DS3231_OFF;  break;
      case '1': value = DS3231_ON;   break;
      default: goto error;
    }
    break;

  case '2':  // type on
    item = DS3231_SQW_TYPE;
    switch (toupper(*param))
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
    switch (toupper(*param))
    {
      case '0': value = DS3231_OFF;  break;
      case '1': value = DS3231_ON;   break;
      default: goto error;
    }
    break;

  case '5':  // alm interrupt operation
    item = DS3231_INT_ENABLE;
    switch (toupper(*param))
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
    switch (toupper(*param))
    {
      case '0': value = DS3231_OFF;  break;
      case '1': value = DS3231_ON;   break;
      default: goto error;
    }
    break;

  case '8':  // enable 32kHz output
    {
      item = DS3231_AGING_OFFSET;
      uint16_t v;

      i2dig(param, v, 2, HEX);
      value = (codeStatus_t)v;
    }
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

const MD_cmdProcessor::cmdItem_t PROGMEM cmdTable[] =
{
  { "?",   handlerHelp, "",    "Help", 0 },
  { "h",   handlerHelp, "",    "Help", 0 },
  { "tw",  handlerTW,   "t",   "writes specifed time spec (see below)", 1 },
  { "tr",  handlerTR,   "",    "read the current time", 1 },
  { "rr",  hanlderRR,   "",    "read the IC registers as raw RAM", 1 },
  { "s",   handlerS,    "",    "status of the RTC", 1 },
  { "d",   handlerD,    "",    "calculate day of week from current date", 1 },
  { "a1i", handlerA1I,  "v",   "alm1 int ena (0=dis, 1=ena)", 2 },
  { "a1f", handlerA1F,  "",    "alm1 flag reset", 2 },
  { "a1t", handlerA1T,  "a",   "alm1 set alm type (see below)", 2 },
  { "a1z", handlerA1Z,  "",    "alm1 zero all values", 2 },
  { "a1w", handlerA1W,  "at",  "alm1 write alm type a & time spec t (see below)", 2 },
  { "a2i", handlerA2I,  "v",   "alm2 int ena (0=dis, 1=ena)", 3 },
  { "a2f", handlerA2F,  "",    "alm2 flag reset", 3 },
  { "a2t", handlerA2T,  "v",   "alm2 set alm type (see below)", 3 },
  { "a2z", handlerA2Z,  "",    "alm2 zero all values", 3 },
  { "a2w", handlerA2W,  "at",  "alm1 write alm type a & time spec t (see below)", 3 },
  { "c"  , handlerC,    "nv",  "control status n, write value v, where n,n are:", 4 }
};

MD_cmdProcessor CP(Serial, cmdTable, ARRAY_SIZE(cmdTable));

void handlerHelp(char *param)
{
  Serial.print(F("\nHelp\n----"));
  CP.help();
  // Expand on configuration items
  PRINTS("\n\t0 - Clock Halt (v 0=run, 1=halt)");
  PRINTS("\n\t1 - BB SQW Enable(v 0=halt, 1=run)");
  PRINTS("\n\t2 - SQ Wave Freq (v 1=1Hz, 2=1khz, 3=4khz, 4=8kHz)");
  PRINTS("\n\t3 - TCXO conversion (v unused)");
  PRINTS("\n\t4 - 12 hour mode (v 0=24h, 1=12h)");
  PRINTS("\n\t5 - Alm interrupt operation (v 0=disabled, 1=enabled)");
  PRINTS("\n\t6 - Reset Halted Flag (v unused)");
  PRINTS("\n\t7 - Enabled 32kHz output (v 0=disabled, 1=enabled)");
  PRINTS("\n\t8 - Set aging register (v hex value)");
  PRINTS("\n");
  PRINTS("\nTime specification is yyyymmddhhnnssw (24 hr clock, w 1=Sun..7=Sat)");
  PRINTS("\nAlarm Type specification is 0=all s, 1=s, 2=s+m, 3=s+m+h, 4=s+m+h+dt, 5=s+m+h+dy");
  PRINTS("\n");
}

void setup(void)
{
  Serial.begin(57600);
  PRINTS("\n[MD_DS3231_Tester]");
  
  RTC.setAlarm1Callback(cbAlarm1);
  RTC.setAlarm2Callback(cbAlarm2);

  CP.begin();
  handlerHelp(nullptr);
}

void loop(void)
{
   RTC.checkAlarm1();
   RTC.checkAlarm2();
   CP.run();
 }

