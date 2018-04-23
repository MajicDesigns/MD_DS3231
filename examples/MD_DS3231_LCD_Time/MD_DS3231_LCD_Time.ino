// Example code for the MD_DS3231 library
//
// Implements a clock display on a 16x2 LCD shield.
// Display updates are triggered by a 1 second alarm from the RTC alarm 1.
//
// The example shows using the library in polled mode, polled mode with callback, 
// or interrupt mode. Different modes of operation are enabled through setting 
// one of the USE_* defines at the top of the file.
//
#include <LiquidCrystal.h>
#include <MD_DS3231.h>
#include <Wire.h>

// Select the mode of operation for the library
#define USE_POLLED      0 // polled only mode - use the return status from checkAlarm()
#define USE_POLLED_CB   1 // polled mode with callback - checkAlarm() will invoke the callback
#define USE_INTERRUPT   0 // external interrupt operation, set up for Arduino Uno pin PIN_INTERRUPT

#define PIN_INTERRUPT   2 // interrupt pin

LiquidCrystal lcd(8, 9, 4, 5, 6, 7);

volatile boolean  bShowTime = false;

void p2dig(uint8_t v)
// print 2 digits with leading zero
{
  if (v < 10) lcd.print("0");
  lcd.print(v);
}

const char *dow2String(uint8_t code)
// Day of week to string. DOW 1=Sunday, 0 is undefined
{
  static const char *str[] = {"---", "Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat"};
  
  if (code > 7) code = 0;
  return(str[code]);
}

void printTime(void)
// Print the current time and date to the LCD display
{
  lcd.setCursor(0,0);
  lcd.print(dow2String(RTC.dow));
  lcd.print(" ");  
  lcd.print(RTC.yyyy);
  lcd.print("-");
  p2dig(RTC.mm);
  lcd.print("-");
  p2dig(RTC.dd);
  
  lcd.setCursor(0,1);
  p2dig(RTC.h);
  lcd.print(":");
  p2dig(RTC.m);
  lcd.print(":");
  p2dig(RTC.s);
  if (RTC.status(DS3231_12H) == DS3231_ON)
    lcd.print(RTC.pm ? " pm" : " am");
}

void displayUpdate(void)
// update the display
{
  RTC.readTime();
  printTime();
}

#if USE_POLLED_CB || USE_INTERRUPT
void alarmICB(void)
// callback and interrupt function (same functionality)
{
  bShowTime = true;    // set the flag to update
}
#endif

void setup()
{
  // initialise 16x2 LCD display shield
  lcd.begin(16,2);
  lcd.clear();
  lcd.noCursor();

  /// set up the alarm environment  
#if USE_POLLED
  // nothing more to do here
#endif
#if USE_POLLED_CB
  RTC.setAlarm1Callback(alarmICB);
#endif
#if USE_INTERRUPT
  // set up hardware at Arduino end
  pinMode(PIN_INTERRUPT, INPUT_PULLUP);
  attachInterrupt(0, alarmICB, FALLING);
  // set up the clock interrupt registers
  RTC.control(DS3231_A2_INT_ENABLE, DS3231_ON);
  RTC.control(DS3231_A1_INT_ENABLE, DS3231_ON);  
  RTC.control(DS3231_INT_ENABLE, DS3231_ON);
  // set the Alarm flag off in case it is on
  RTC.control(DS3231_A1_FLAG, DS3231_OFF);
#endif

  // now initialise the 1 second alarm for screen updates
  RTC.setAlarm1Type(DS3231_ALM_SEC);
}

void loop()
{
#if USE_POLLED
  bShowTime = RTC.checkAlarm1();
#endif
#if USE_POLLED_CB
  RTC.checkAlarm1();
#endif

  // if the flag has been set, update the display then reset the show flag
  if (bShowTime) 
  {
#if USE_INTERRUPT
    RTC.control(DS3231_A1_FLAG, DS3231_OFF);  // clear the alarm flag
#endif
    displayUpdate();
  }    
  bShowTime = false;
}

