#include "display.h"
#include "Nextion.h"
#include <ESPAsyncWebServer.h> // https://github.com/me-no-dev/ESPAsyncWebServer
#include <TimeLib.h>
#include <ESP8266mDNS.h> // for WiFi.RSSI()
#include "eeMem.h"
#include "WiFiManager.h"

Nextion nex;
extern WiFiManager wifi;
extern void changeTemp(int delta, bool bAll);
extern void Tone(unsigned int frequency, uint32_t duration);
extern void CallHost(reportReason r);
extern void LightSwitch(uint8_t t, uint8_t v);

void Display::init()
{
  if(wifi.isCfg() ) // don't interfere with SSID config
    return;
  nex.FFF(); // Just to end any debug strings in the Nextion
  nex.reset();
  screen( true ); // brighten the screen if it just reset
  nex.autoWake(true);
  refreshAll();
}

// called each second
void Display::oneSec()
{
  if(wifi.isCfg() )
    return;
  refreshAll();    // time update every seconds
  if(nex.getPage() == Page_Main)
  {
    updateRSSI();     //
    if( m_backlightTimer ) // the dimmer thing
    {
      if(--m_backlightTimer == 0)
        screen(false);
    }
  }
}

bool Display::checkNextion() // all the Nextion recieved commands
{
  static char cBuf[66];
  int len = nex.service(cBuf); // returns just the button value or 0 if no data
  uint8_t btn;
  uint32_t val;
  String s;
  static uint8_t textIdx = 0;
  bool bRtn = false;

  if(len == 0)
  {
    if(m_btnMode)
      if(--m_btnDelay <= 0)
      {
        buttonRepeat();
        m_btnDelay = 40; // repeat speed
      }
    return false;
  }

  switch(cBuf[0])  // code
  {
    case 0x65: // button
      bRtn = true; // anything pressed
      btn = cBuf[2];
      if( m_backlightTimer == 0)
      {
        nex.brightness(NEX_BRIGHT); // backlight was off, ignore this input
        m_backlightTimer = NEX_TIMEOUT;
        return bRtn;
      }
      if(cBuf[3]) // press, not release
        Tone(6000, 20);

      switch(cBuf[1]) // page
      {
        case Page_Main:
          m_backlightTimer = NEX_TIMEOUT;
          switch(btn)
          {
            case 0: // screen (not used)
              break;
            case 1: // TIME
              break;
            case 4: // alarm
              m_bAlarmOn = !m_bAlarmOn;
              refreshAll();
              break;
            case 6: // temp up
              if(ee.bVaca) // first tap, disable vacation mode
                ee.bVaca = false;
              else
              {
                if(cBuf[3])
                {
                  m_btnMode = 1;
                  buttonRepeat();
                  m_btnDelay = 120;
                }
                else m_btnMode = 0;
              }
              break;
            case 7: // temp down
              if(ee.bVaca) // disable vacation mode
                ee.bVaca = false;
              else
              {
                if(cBuf[3])
                {
                  m_btnMode = 2;
                  buttonRepeat();
                  m_btnDelay = 120;
                }
                else m_btnMode = 0;
              }
              break;
            case 5: // dimmer slider
              if(cBuf[3]) // press
              {
                nex.getVal(0);
                m_bSliderDn = true;
              }
              else m_bSliderDn = false;
              break;
            case 14: // Light
              m_bLightOn = !m_bLightOn;
              m_LightSet = 1;
              break;
            case 17: // Alarm page
              nex.setPage(Page_Alarms); // set alarms
              updateAlarms();
              break;
            case 18: // Schedule page
              nex.sleep(true);
              break;
            case 19: // Thermostat page
              break;
          }
          break;
        case Page_Thermostat:
          break;
        case Page_SSID: // Selection page t1=ID 2 ~ t16=ID 17
          wifi.setSSID(cBuf[2]-2);
          nex.refreshItem("t0"); // Just to terminate any debug strings in the Nextion
          nex.setPage(Page_Keyboard); // go to keyboard
          nex.itemText(1, "Enter Password");
          textIdx = 2; // AP password
          break;

        case Page_Alarms:
          switch(btn)
          {
            case 1: // up
              if(cBuf[3])
              {
                m_btnMode = 1;
                buttonRepeat();
                m_btnDelay = 120;
              }
              else m_btnMode = 0;
              break;
            case 2: // down
              if(cBuf[3])
              {
                m_btnMode = 2;
                buttonRepeat();
                m_btnDelay = 120;
              }
              else m_btnMode = 0;
              break;
            case 3: // hour, minute, Am/Pm
            case 4:
            case 5:
            case 6:
            case 7:
            case 8:
            case 9:
            case 10:
            case 11:
            case 12:
            case 13:
            case 14:
            case 15:
            case 16:
            case 17:
              s = String("t") + m_almSelect; // change color back
              nex.itemColor(s, rgb16(0, 63, 31));
              m_almSelect = btn - 3;
              s = String("t") + m_almSelect; // change to new selection
              nex.itemColor(s, rgb16(31, 63, 31));
              updateAlarms();
              break;
            case 19: // checkboxes
            case 20:
            case 21:
            case 22:
            case 23:
              ee.alarm[btn-19].wday = ee.alarm[btn-19].wday ? 0:1;
              break;
            case 18: // Main
              nex.setPage(Page_Main);
              delay(25);
              refreshAll();
              break;
          }
          break;

        default: // all pages go back
//          screen(true);
          break;
      }
      break;
    case 0x70:// string return from keyboard
      switch(textIdx)
      {
        case 0: // zipcode edit
//          if(strlen(cBuf + 1) < 5)
//            break;
//          strncpy(ee.zipCode, cBuf + 1, sizeof(ee.zipCode));
          break;
        case 1: // password edit
//          if(strlen(cBuf + 1) < 5)
//            return;
//          strncpy(ee.password, cBuf + 1, sizeof(ee.password) );
          break;
        case 2: // AP password
          nex.setPage(Page_Main);
          wifi.setPass(cBuf + 1);
          break;
      }
      screen(true); // back to main page
      break;
    case 0x71: // numeric data
      val = cBuf[1] | (cBuf[2] << 8) | (cBuf[3] << 16) | (cBuf[4] << 24);
      switch(nex.m_valItem)
      {
        case 0: // light level
          bool bSend = (m_nLightLevel != val) ? true:false;
          m_nLightLevel = val;
          if(bSend)
            m_LightSet = 2;
          if(m_bSliderDn)
            nex.getVal(nex.m_valItem);
          break;
      }
      break;
  }
  return bRtn;
}

void Display::updateAlarms()
{
  String s;
  for(int i = 0; i < 5; i++)
  {
    uint16_t v = ee.alarm[i].timeSch;
    uint8_t h = v / 60;
    nex.itemText(i+10, (h > 11) ? "PM":"AM");
    if(h == 0) h =12;
    else if(h > 12) h -= 12;
    s = h;
    s += ":";
    nex.itemText(i, s);
    uint8_t m = v % 60;
    s = "";
    if(m < 10) s = "0";
    s += m;
    nex.itemText(i+5, s);
    nex.checkItem(i, ee.alarm[i].wday ? 1:0);
    s = String("t") + i;
  }
}

void Display::buttonRepeat()
{
  uint8_t alm;
  uint8_t sel;
  uint8_t m;
  String s;

  switch(nex.getPage())
  {
    case Page_Main:
      changeTemp((m_btnMode==1) ? 1:-1, true);
      refreshAll();
      break;
    case Page_Alarms:
      alm = m_almSelect % 5;
      sel = m_almSelect / 5;
      if(m_btnMode == 1) // up
      {
        switch(sel)
        {
          case 0: // hour
            ee.alarm[alm].timeSch += 60;
            ee.alarm[alm].timeSch %= 24*60;
            break;
          case 1:
            if(++ee.alarm[alm].timeSch >= 24*60)
              ee.alarm[alm].timeSch = 0;
            break;
          case 2: // AM/PM
            ee.alarm[alm].timeSch += 12*60;
            ee.alarm[alm].timeSch %= 24*60;
            break;
        }
      }
      else switch(sel)
      {
          case 0: // hour
            ee.alarm[alm].timeSch -= 60;
            ee.alarm[alm].timeSch %= 24*60;
            break;
          case 1:
            if(--ee.alarm[alm].timeSch < 0)
              ee.alarm[alm].timeSch = 24*60 - 1;
            break;
          case 2: // AM/PM
            ee.alarm[alm].timeSch -= 12*60;
            ee.alarm[alm].timeSch %= 24*60;
            break;
      }
      switch(sel)
      {
        case 0: // hour
          m = ee.alarm[alm].timeSch / 60;
          if(m  == 0) m = 12;
          s = m;
          s += ":";
          nex.itemText(m_almSelect, s);
          break;
        case 1: // minute
          s = "";
          m = ee.alarm[alm].timeSch % 60;
          if(m < 10) s = "0";
          s += m;
          nex.itemText(m_almSelect, s);
          break;
        case 2: // AM/PM
          nex.itemText(m_almSelect, (ee.alarm[alm].timeSch) > (11*60) ? "PM":"AM");
          break;
      }
      break;
  }
  Tone(6000, 20);
}

String Display::fmtTime(uint16_t v)
{
  String s;
  uint8_t h = v / 60;
  bool bPM = (h > 11) ? true:false;
  if (h > 12)
    h -= 12;
  if(h == 0)
    h = 12;
  if(h < 10) s += " ";
  s += h;
  s += ":";
  uint8_t m = v % 60;
  if(m < 10) s += "0";
  s += m;
  s += bPM ? " PM":" AM";
  return s;
}

const char *_days_short[] = {"Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat"};
const char *_mon[] = {"","JAN", "FEB", "MAR", "APR", "MAY", "JUN", "JUL", "AUG", "SEP", "OCT", "NOV", "DEC"};

void Display::refreshAll()
{
  static int save[7];

  if(nex.getPage())
  {
    save[0] = -1;
    save[5] = -1;
    save[6] = -1;
    return;  // t7 and t8 are only on thermostat (for now)
  }

  String sTime;

  if(minute() != save[0])
  {
    save[0] = minute();
    sTime = String( hourFormat12() );
    sTime += ":";
    if(minute() < 10) sTime += "0";
    sTime += minute();
    nex.itemText(0, sTime);
    sTime = _mon[month()];
    sTime += " ";
    sTime += day();
    nex.itemText(2, sTime);
/*  if(weekday() != lastDay)   // update weekday
  {
    lastDay = weekday();
    nex.itemText(7, _days_short[weekday()-1]);
  }*/
    nex.itemText(13, isPM() ? "PM":"AM");
  }
  sTime = "";
  if(second() < 10) sTime += "0";
  sTime += second();
  nex.itemText(1, sTime);

  if(save[1] != m_currentTemp)
  {
    save[1] = m_currentTemp;
    nex.itemText(5, String((float)m_currentTemp/10, 1) );
  }
  if(save[2] != m_hiTemp)
  {
    save[2] = m_hiTemp;
    nex.itemText(4, String((float)m_hiTemp/10, 1) );
  }
  if(save[3] != m_roomTemp)
  {
    save[3] = m_roomTemp;
    nex.itemText(8, String((float)m_roomTemp/10, 1) + " " );
  }
  if(save[4] != m_rh)
  {
    save[4] = m_rh;
    nex.itemText(9, String((float)m_rh/10, 1) + "%" );
  }
  if((bool)save[5] != m_bAlarmOn)
  {
    save[5] = m_bAlarmOn;
    sTime = "ALARM ";
    sTime += fmtTime(ee.alarm[0].timeSch);
    sTime += m_bAlarmOn ? " ON " : " OFF";
    nex.itemText(3, sTime );
    nex.itemColor("t3", m_bAlarmOn ? rgb16(31,0,0):rgb16(15,0,31) );
  }
  if(save[6] != m_bHeater)
  {
    save[6] = m_bHeater;
    nex.itemColor("t5", m_bHeater ? rgb16(31,0,0):rgb16(15,0,31) );
  }
  if(save[7] != m_schInd)
  {
    save[7] = m_schInd;
    sTime = ee.bVaca ? "Vacation" : ee.schedule[m_schInd].name;
    nex.itemText(12, sTime );
  }
}

bool Display::checkAlarms()
{
  if(m_bAlarmOn == false)
    return false;
  uint16_t tm = (hour() * 60) + minute();
  for(int i = 0; i < 5; i++)
  {
    if(ee.alarm[i].wday && ee.alarm[i].timeSch == tm) // wday is just active or not for now
      return true;
  }
  return false;
}

// Set slider to dimmer level
void Display::updateLevel(uint8_t lvl)
{
  m_nLightLevel = lvl;
  nex.setVal(0, lvl);
}

// true: set screen backlight to bright plus switch to main
// false: switch to blank and dim
bool Display::screen(bool bOn)
{
  if(wifi.isCfg() )
    return false;
  static bool bOldOn = true;

  if(bOldOn && nex.getPage()) // not in sync
    bOldOn = false;

  if(bOn) // input or other reason
  {
    nex.brightness(NEX_BRIGHT);
    m_backlightTimer = NEX_TIMEOUT; // update the auto backlight timer
    if( bOn == bOldOn )
      return false; // no change occurred
    if(nex.getPage())
    {
      nex.setPage(Page_Main);
      delay(25);
    }
    refreshAll();
  }
  else
  {
      nex.brightness(NEX_OFF);
  }
  bOldOn = bOn;
  return true; // it was changed
}

// return true if backlight is off
bool Display::isOff()
{
  return (m_backlightTimer == 0) ? true:false;
}

void Display::updateRSSI()
{
  static uint8_t seccnt = 2;
  static int16_t rssiT;
#define RSSI_CNT 8
  static int16_t rssi[RSSI_CNT];
  static uint8_t rssiIdx = 0;

  if(nex.getPage()) // must be page 0
  {
    rssiT = 0; // cause a refresh later
    seccnt = 1;
    return;
  }
  if(--seccnt)
    return;
  seccnt = 3;     // every 3 seconds

  rssi[rssiIdx] = WiFi.RSSI();
  if(++rssiIdx >= RSSI_CNT) rssiIdx = 0;

  int16_t rssiAvg = 0;
  for(int i = 0; i < RSSI_CNT; i++)
    rssiAvg += rssi[i];

  rssiAvg /= RSSI_CNT;
  if(rssiAvg == rssiT)
    return;

  nex.itemText(7, String(rssiT = rssiAvg) + "dB");

  int sigStrength = 127 + rssiT;
  int wh = 24; // width and height
  int x = 12; // X/Y position
  int y = 198;
  int sect = 127 / 5; // 25
  int dist = wh  / 5; // distance between blocks

  y += wh;
  static uint8_t oldBars;
  uint8_t bars = sigStrength / sect;

  if(bars == oldBars)
    return;
  oldBars = bars;
  for (int i = 1; i < 6; i++)
  {
    nex.fill( x + i*dist, y - i*dist, dist-2, i*dist, (sigStrength > i * sect) ? rgb16(0, 63,31) : rgb16(5, 10, 5) );
  }
}
