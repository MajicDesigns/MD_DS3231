// Example code for the MD_DS3231 library
//
// Implements an alarm clock on a 16x2 LCD shield with analog tact switches.
//
// Alarm 1 is used in 'every second' mode and updates the display through a callback function.
// Alarm 2 is polled and action taken when the alarm is detected.
//
// Alarm can be set by using the Select key, then LEFT or RIGHT to increment hr or min, then SELECT to write the value.
// Alarm is toggled on/off using the UP key
// Alarm triggered will play a tone on a piezo speaker on BUZZER_PIN.
//
// MD_AButton library located at https://github.com/MajicDesigns/MD_AButton
//
#include <LiquidCrystal.h>
#include <MD_DS3231.h>
#include <MD_AButton.h>
#include <Wire.h>

// Define as 1 if alarm time to be initialised from variables, otherwise use RTC values
#define INIT_ALM_DEFAULTS 0

// Hardware definitions
#define BUZZER_PIN  2               // digital pin for tone buzzer
#define ANALOG_SW   KEY_ADC_PORT    // analog value for LCD keypad

// Global objects
LiquidCrystal lcd(8, 9, 4, 5, 6, 7);
MD_AButton    lcdKeys(ANALOG_SW);

// Alarm setting registers
uint8_t almH = 7, almM = 0;         // alm values, including defaults
boolean almEnabled = true;
#define ALARM_ON_TYPE   DS3231_ALM_HM     // type of alarm set to turn on
#define ALARM_OFF_TYPE  DS3231_ALM_DDHM   // type of alarm set to turn off

// LCD characters to show alarm status
#define ALM_SET_CHAR  0x01
#define ALM_OFF_CHAR  0xdb
#define ALM_ON_CHAR   0x02

// Alarm state machine values
enum almFSM_t { AS_IDLE, AS_SETTING, AS_TRIGGERED };
almFSM_t almState = AS_IDLE;

void displayUpdate(void)
// Callback function for the RTC library
// Display the RTC values for time and alarm2
{
  RTC.readTime();
  printTime();
  if (almState != AS_SETTING)
  {
    RTC.readAlarm2();
    almH = RTC.h;
    almM = RTC.m;
    printAlarm();
  }
}

void p2dig(uint8_t v)
// Print 2 digits with leading zero
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

void printAlarm(void)
// Display the alarm setting on the display
{
  // Status indicator
  lcd.setCursor(15,0);
  if (almState == AS_SETTING)
    lcd.write((char)ALM_SET_CHAR);
  else if (almEnabled)
    lcd.write((char)ALM_ON_CHAR);
  else 
    lcd.write((char)ALM_OFF_CHAR);
    
  // Alarm time
  lcd.setCursor(11,1);  
  p2dig(almH);
  lcd.print(":");
  p2dig(almM);  
}

void printTime(void)
// Display the current time on the display
{
  // display the date and day
  lcd.setCursor(0,0);
  lcd.print(dow2String(RTC.dow));
  lcd.print(" ");
  lcd.print(RTC.yyyy);
  lcd.print("-");
  p2dig(RTC.mm);
  lcd.print("-");
  p2dig(RTC.dd);
  
  // display the current time  
  lcd.setCursor(0,1);
  p2dig(RTC.h);
  lcd.print(":");
  p2dig(RTC.m);
  lcd.print(":");
  p2dig(RTC.s);
  if (RTC.status(DS3231_12H) == DS3231_ON)
    lcd.print(RTC.pm ? " pm" : " am");
}

void setMyAlarm(void)
// Set Alarm 2 values in the RTC 
{
  RTC.yyyy = RTC.mm = RTC.dd = 0;
  RTC.h = almH;
  RTC.m = almM;
  RTC.s = RTC.dow = 0;
  if (almEnabled)
    RTC.writeAlarm2(ALARM_ON_TYPE);
  else
    RTC.writeAlarm2(ALARM_OFF_TYPE);
}  

void doBuzzAlert()
// Buzz piezo buzzer, not much to do here
{
  tone(BUZZER_PIN, 800, 400);
  delay(400);
  tone(BUZZER_PIN, 690, 400);
}

void setup()
{
  uint8_t  c[8];
  
  // initialise 16x2 LCD display shield
  lcd.begin(16,2);
  lcd.clear();
  lcd.noCursor();
  
  // create alarm status characters
  c[0] = c[7] = 0x00;
  c[1] = c[6] = 0x1f;
  c[2] = c[3] = c[4] = c[5] = 0x15;
  lcd.createChar(ALM_SET_CHAR, c);
  c[2] = c[3] = c[4] = c[5] = 0x1f;
  lcd.createChar(ALM_ON_CHAR, c);

  // Buzzer
  pinMode(BUZZER_PIN, OUTPUT);

  // RTC global initialisation
  RTC.control(DS3231_12H, DS3231_OFF);  // 24 hour clock
  
  // Alarm 1 - initialise the 1 second alarm for screen updates
  RTC.setAlarm1Callback(displayUpdate);
  RTC.setAlarm1Type(DS3231_ALM_SEC);
  
  // Alarm 2 - initialise the alarm
#if INIT_ALM_DEFAULTS
  setMyAlarm();
#else 
  RTC.setAlarm2Type(ALARM_OFF_TYPE);
#endif
   
  almState = AS_IDLE;
  
  displayUpdate();
}

void loop()
{
  char	c = lcdKeys.getKey();

  // screen update through callback every second
  RTC.checkAlarm1();

  // If the alarm has gone off, overrides any other state
  if (RTC.checkAlarm2()) 
    almState = AS_TRIGGERED;
    
  switch (almState) // Alm Finite State Machine
  {
    case AS_IDLE:     // IDLE state - usually in this state 
      switch (c)
      {
        case 'S':     // Select has been pressed, ALM setting mode has been requested
          almState = AS_SETTING;
          RTC.readAlarm2(); // get the current values into the temp values, even if we do this every screen update
          almH = RTC.h;
          almM = RTC.m;
          printAlarm();
          break;
        
        case 'U':     // Up has been pressed, ALM on/off toggle
          almEnabled = !almEnabled;
          if (almEnabled) 
            RTC.setAlarm2Type(ALARM_ON_TYPE);
          else
            RTC.setAlarm2Type(ALARM_OFF_TYPE);
          printAlarm();
          break;
      }        
      break;
      
    case AS_SETTING:  // SETTING state - process the keys
      switch (c)
      {
        case 'S':     // Select key - conclude setting mode and write the new alm time
          almState = AS_IDLE;
          setMyAlarm();       
          break;
          
        case 'L':     // Left key - increment the alm hour
          almH = (almH + 1) % 24;
          printAlarm();
          break;
      
        case 'R':     // Right Key - increment the alm minute
          almM = (almM + 1) % 60;
          printAlarm();
          break;
      }       
      break;
      
    case AS_TRIGGERED:  // TRIGGERED state - do whatever to signal alarm
      doBuzzAlert();
      almState = AS_IDLE;
      break;
      
    default:
      almState = AS_IDLE;
  }
}

