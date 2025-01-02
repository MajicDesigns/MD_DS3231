// Example code for the MD_DS3231 library
//
// Implements a clock display on a Round TFT LCD display.
// 
// Hardware Requirements
// ---------------------
// - DS3231 clock modules with a I2C interface.
// - GC9A01 240x240 round LCTFT LCD module
// - 2 momentary-on tact switches
//
// Description:
// ------------
// Time is kept by the DS3231 Real Time Clock (RTC) module.
// The display is implemented using a graphical display on the TFT LCD.
// Tact (MODE and SET) switches are used to set the time on the display.
//
// Functions:
// ----------
// To set up the time:
//  + Click the MODE switch
//  + Click the SET switch to advance the clock hand for the enabled element
//  + Click the MODE switch to change element and repeat step above 
//     - elements progress H, M.
//  + Clicking the MODE switch after the last element (M) sets the new time.
// 
// Setup mode has a SET_TIMEOUT for no inactivity. On timeout it abandons any changes.
// 
// Long press the MODE switch toggles summer time mode for easy set/reset.
// Press the SET switch to show full scale on all meters, release to show time.
//
// More details at 
//
// Library dependencies:
// ---------------------
// MD_DS3231 RTC libraries found at https://github.com/MajicDesigns/DS3231 or the Arduino
// IDE Library Manager. Any other RTC may be substituted with few changes as the current 
// time is passed to all display functions.
//
// MD_UISwitch library is found at https://github.com/MajicDesigns/MD_UISwitch or the 
// Arduino IDE Library Manager.
//

#include <Wire.h>         // I2C library for RTC comms
#include <Adafruit_GFX.h> // Graphics routines
#include <Adafruit_GC9A01A.h> // TFT LCD hardware specific
#include <EEPROM.h>       // Summer time status stored in EEPROM
#include <MD_UISwitch.h>  // Switch manager
#include <MD_DS3231.h>    // RTC module manager
#include "HtmlColors.h"

#define DEBUG 1

#ifdef DEBUG
#define PRINTS(s)                 do { Serial.print(F(s)); } while(false);
#define PRINT(s, v)               do { Serial.print(F(s)); Serial.print(v); } while(false);
#define PRINTTIME(s, hh, mm, ss)  do { Serial.print(F(s)); Serial.print(hh); Serial.print(F(":")); Serial.print(mm); Serial.print(F(":")); Serial.print(ss); } while(false);
#else
#define PRINTS(s)
#define PRINT(s, v)
#define PRINTTIME(s, hh, mm, ss)
#endif

// --------------------------------------
// Hardware definitions

const uint8_t PIN_MODE = 7; // setup mode switch
const uint8_t PIN_SET = 8;  // setup setting switch

// LCD display SPI connections
// RST  No connection
// CS   D10  SS
// DC   D9  
// SDA  D11  MOSI
// SCL  D13  SCLK
//
const uint8_t TFT_DC = 9;
const uint8_t TFT_CS = 10;
Adafruit_GC9A01A tft(TFT_CS, TFT_DC);

const uint8_t EE_SUMMER_FLAG = 0; // EEPROM address for this value

// --------------------------------------
// Miscellaneous defines

// Time constants
const uint8_t  SEC_PER_MIN = 60;   // seconds in a minute
const uint8_t  MIN_PER_HR = 60;    // minutes in an hour
const uint8_t  HR_PER_DAY = 24;    // hours per day
const uint8_t  DAY_PER_WK = 7;     // days per week
const uint8_t  MTH_PER_YEAR = 12;  // months per year

const uint32_t CLOCK_UPDATE_TIME = 1000;  // in milliseconds - time resolution to nearest minute does not need rapid updates!
const uint32_t SETUP_TIMEOUT = 60000;     // in milliseconds - timeout for setup inactivity
const uint32_t SHOW_DELAY_TIME = 5000;    // in milliseconds - timeout for showing time mode
const uint32_t INIT_CHECK_TIME = 1000;    // in milliseconds - initialization check per display

// Clock face colors
const uint16_t COL_BACK = RGB565::Black;
const uint16_t COL_BORDER = RGB565::Firebrick;
const uint16_t COL_H_TICK = RGB565::Yellow;
const uint16_t COL_M_TICK = RGB565::White;
const uint16_t COL_Q_TICK = RGB565::White;
const uint16_t COL_AXLE = RGB565::White;
const uint16_t COL_S_HAND = RGB565::Orange;
const uint16_t COL_M_HAND = RGB565::Silver;
const uint16_t COL_H_HAND = RGB565::Silver;

// Clock element size parameters in pixels
const uint16_t CTR_X = tft.width() / 2;   // center x position
const uint16_t CTR_Y = tft.height() / 2;  // center y position

const uint16_t R_OUTER = (tft.width() / 2) - 2;
const uint8_t  W_OUTER = 8;

const uint16_t R_H_TICK = R_OUTER - 4;
const uint8_t  L_H_TICK = 14;
const uint16_t R_M_TICK = R_OUTER - 16;
const uint8_t  L_Q_TICK = 2;
const uint8_t  R_AXLE = 3;
const uint8_t  R_H_HAND = 62;
const uint8_t  R_M_HAND = 84;
const uint8_t  R_S_HAND = 90;

// --------------------------------------
//  END OF USER CONFIGURABLE INFORMATION
// --------------------------------------

#define ARRAY_SIZE(a) (sizeof(a)/sizeof((a)[0]))

#ifdef DEBUG
#define PRINTS(s)                 do { Serial.print(F(s)); } while(false);
#define PRINT(s, v)               do { Serial.print(F(s)); Serial.print(v); } while(false);
#define PRINTTIME(s, hh, mm, ss)  do { Serial.print(F(s)); Serial.print(hh); Serial.print(F(":")); Serial.print(mm); Serial.print(F(":")); Serial.print(ss); } while(false);
#else
#define PRINTS(s)
#define PRINT(s, v)
#define PRINTTIME(s, hh, mm, ss)
#endif

// Data Types
typedef struct
{
  float sx, sy;   // x, y unit circle cartesian coordinates
  int16_t x, y;   // x, y actual cartesian coordinates
} point_t;

// Angle constants and conversions
const uint16_t DEG_PER_REV = 360;
const float RAD_PER_REV = (2*PI);

inline float deg2rad(int16_t d) { return  (d * RAD_PER_REV) / DEG_PER_REV; }

// Global variables
MD_UISwitch_Digital  swMode(PIN_MODE);  // mode/setup switch handler
MD_UISwitch_Digital  swSet(PIN_SET);    // setting switch handler

// --------------------------------------
// Code
bool isSummerMode(void)
// Return true if summer mode is active
{
  return(EEPROM.read(EE_SUMMER_FLAG) != 0);
}

void flipSummerMode(void)
// Reverse the the summer flag mode in the EEPROM
{
  // handle EEPROM changes
  EEPROM.write(EE_SUMMER_FLAG, isSummerMode() ? 0 : 1);
  PRINT("\nNew Summer Mode ", isSummerMode());
}

uint8_t adjustedHour(uint8_t h)
// Change the RTC hour to include any summer time offset
// Clock always holds the 'real' time.
{
  h += (isSummerMode() ? 1 : 0);
  if (h > HR_PER_DAY / 2) h -= HR_PER_DAY / 2;

  return(h);
}

void drawDial(void)
{
  point_t p0, p1;

  // clear the display
  tft.fillScreen(COL_BACK);

  // Outer ring
  tft.fillCircle(CTR_X, CTR_Y, R_OUTER, COL_BORDER);
  tft.fillCircle(CTR_X, CTR_Y, R_OUTER - W_OUTER, COL_BACK);

  // Draw 12 hour markers, use degrees to get coordinates
  for (auto d = 0; d < DEG_PER_REV; d += DEG_PER_REV / 12)
  {
    float r = deg2rad(d - (DEG_PER_REV / 4));

    p0.sx = p1.sx = cos(r);
    p0.sy = p1.sy = sin(r);

    p0.x = p0.sx * R_H_TICK;
    p0.y = p0.sy * R_H_TICK;

    p1.x = p1.sx * (R_H_TICK - L_H_TICK);
    p1.y = p1.sy * (R_H_TICK - L_H_TICK);

    tft.drawLine(p0.x + CTR_X, p0.y + CTR_Y, p1.x + CTR_X, p1.y + CTR_Y, COL_H_TICK);
  }

  // Draw 60 hour/minute markers as dots, use degrees to get coordinates
  for (auto d = 0; d < DEG_PER_REV; d += DEG_PER_REV / 60)
  {
    float r = deg2rad(d);

    p0.x = cos(r) * R_M_TICK;
    p0.y = sin(r) * R_M_TICK;

    tft.drawPixel(p0.x + CTR_X, p0.y + CTR_Y, COL_M_TICK);

    // Enlarge 4 main main quadrant dots
    if (d == 0 || d == DEG_PER_REV / 4 || d == DEG_PER_REV / 2 || d == (DEG_PER_REV * 3) / 4)
      tft.fillCircle(p0.x + CTR_X, p0.y + CTR_Y, L_Q_TICK, COL_Q_TICK);
  }

  // central axle 
  tft.fillCircle(CTR_X, CTR_Y, R_AXLE, COL_AXLE);
}

void drawHands(uint8_t hh, uint8_t mm, uint8_t ss, bool initialize = false)
{
  float angS, angM, angH;     // H, M, S hands angles in radians
  static point_t pS, pM, pH;  // point coordinates for H, M, S

  // compute current hand angles from the time
  angS = ss * (RAD_PER_REV / 60.0);                   // 0-60 sec -> 0-360 deg
  angM = (mm * (RAD_PER_REV / 60.0)) + (angS / 60.0); // 0-60 min -> 0-360 deg - includes seconds
  angH = (hh * (RAD_PER_REV / 12.0)) + (angM / 12);   // 0-12 hr  -> 0-360 deg - includes minutes and seconds

  // work out new unit circle endpoint coordinates for current positions
  pH.sx = cos(angH);  pH.sy = sin(angH);
  pM.sx = cos(angM);  pM.sy = sin(angM);
  pS.sx = cos(angS);  pS.sy = sin(angS);

  // redraw hour and minute hand positions every minute or when initializing
  if (ss == 0 || initialize)
  {
    // clear old, calculate new, redraw new
    if (!initialize) tft.drawLine(pH.x + CTR_X, pH.y + CTR_Y, CTR_X, CTR_Y, COL_BACK);
    pH.x = pH.sx * R_H_HAND;
    pH.y = pH.sy * R_H_HAND;

    if (!initialize) tft.drawLine(pM.x + CTR_X, pM.y + CTR_Y, CTR_X, CTR_Y, COL_BACK);
    pM.x = pM.sx * R_M_HAND;
    pM.y = pM.sy * R_M_HAND;
  }

  // redraw new hand positions, hour and minute hands not erased here to avoid flicker
  if (!initialize) tft.drawLine(pS.x + CTR_X, pS.y + CTR_Y, CTR_X, CTR_Y, COL_BACK);
  pS.x = pS.sx * R_S_HAND;
  pS.y = pS.sy * R_S_HAND;

  // adjust the drawn lines to erase any artifacts from updates overlaying each other
  tft.drawLine(pS.x + CTR_X, pS.y + CTR_Y, CTR_X, CTR_Y, COL_S_HAND);
  tft.drawLine(pH.x + CTR_X, pH.y + CTR_Y, CTR_X, CTR_Y, COL_H_HAND);
  tft.drawLine(pM.x + CTR_X, pM.y + CTR_Y, CTR_X, CTR_Y, COL_M_HAND);
  tft.drawLine(pS.x + CTR_X, pS.y + CTR_Y, CTR_X, CTR_Y, COL_S_HAND);
  tft.fillCircle(CTR_X, CTR_Y, R_AXLE, COL_S_HAND);
}

bool configTime(uint8_t& h, uint8_t& m)
// Handle the user interface to set the current time.
// Remains in this function until completed (finished == true)
// Return true if the time was fully set, false otherwise (eg, timeout)
{
  bool gotResult = false;
  bool finished = false;
  uint8_t curItem = 0;
  uint32_t  timeLastActivity = millis();

  PRINTS("\nStarting setup");

  do
  {
    yield();          // allow WDT and other things to run if implemented

    if (swMode.read() == MD_UISwitch::KEY_PRESS)
    {
      timeLastActivity = millis();
      curItem++;
      PRINT("\nSetup: ", curItem);

      if (curItem >= 2)
      {
        PRINTS("\nSetup completed");
        finished = gotResult = true;
      }
    }

    // process the current element
    MD_UISwitch::keyResult_t key = swSet.read();

    if (key != MD_UISwitch::KEY_NULL)   // keypress occurred ...
      timeLastActivity = millis();      // ... reset timeout

    switch (curItem)
    {
    case 0:    // minutes
      if (key == MD_UISwitch::KEY_PRESS)
      {
        m++;
        if (m >= MIN_PER_HR) m = 0;
        PRINT("\nM:", m);
        drawHands(adjustedHour(h), m, 0);
      }
      break;

    case 1:    // hours
      if (key == MD_UISwitch::KEY_PRESS)
      {
        h++;
        if (h >= HR_PER_DAY) h = 0;
        PRINT("\nH:", h);
        drawHands(adjustedHour(h), m, 0);
      }
      break;

    default:
      // should never get here, but if we do set to 
      // something sensible that will end the cycle.
      curItem = 99;
      break;
    }

    // check if we timed out
    if (millis() - timeLastActivity >= SETUP_TIMEOUT)
    {
      finished = true;
      PRINTS("\nSetup timeout");
    }
  } while (!finished);

  return(gotResult);
}

void setup(void)
{
#if DEBUG
  Serial.begin(57600);
#endif
  PRINTS("\n[FTF LCD Clock]");

  // set up switch management
  swMode.begin();
  swMode.enableRepeat(false);
  swMode.enableLongPress(true);
  swMode.enableDoublePress(false);

  swSet.begin();
  swSet.enableRepeat(true);
  swSet.enableLongPress(false);
  swSet.enableDoublePress(false);

  // set up RTC management
  RTC.control(DS3231_12H, DS3231_OFF);        // set 24 hr mode
  RTC.control(DS3231_CLOCK_HALT, DS3231_OFF); // start the clock

  // set up the display
  tft.begin();
  tft.setRotation(1);

  PRINT("\nSummer Mode ", isSummerMode());
}

void loop(void)
{
  static bool initHands = true;
  static uint32_t timeLastUpdate = 0;
  static enum stateRun_t    // loop() FSM states
  {
    SR_INIT,          // initialize the display
    SR_UPDATE,        // update the display
    SR_IDLE,          // waiting to update display
    SR_SETUP,         // run the time setup
    SR_SUMMER_TIME,   // toggle summer time indicator
  } state = SR_INIT;

  switch (state)
  {
  case SR_INIT:
    drawDial();
    state = SR_UPDATE;
    break;

  case SR_UPDATE:   // update the display
    timeLastUpdate = millis();
    RTC.readTime();
    PRINTTIME("\n", adjustedHour(RTC.h), RTC.m, RTC.s);
    drawHands(adjustedHour(RTC.h), RTC.m, RTC.s, initHands);
    initHands = false;
    state = SR_IDLE;
    break;

  case SR_IDLE:   // wait for ...
    // ... time to update the display ...
    if (millis() - timeLastUpdate >= CLOCK_UPDATE_TIME)
      state = SR_UPDATE;

    // ... or user input from MODE switch ...
    switch (swMode.read())
    {
    case MD_UISwitch::KEY_PRESS:     state = SR_SETUP; break;
    case MD_UISwitch::KEY_LONGPRESS: state = SR_SUMMER_TIME; break;
    default: break;
    }
    break;

  case SR_SETUP:   // time setup
    if (configTime(RTC.h, RTC.m))
    {
      // write new time to the RTC
      RTC.s = 0;
      RTC.writeTime();
    }
    state = SR_UPDATE;
    break;

  case SR_SUMMER_TIME:  // handle the summer time toggle
    flipSummerMode();
    initHands = true;
    state = SR_INIT;
    break;

  default:
    state = SR_INIT;
    break;
  }
}

