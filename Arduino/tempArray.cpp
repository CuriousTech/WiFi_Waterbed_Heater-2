#include "display.h"
#include "tempArray.h"
#include "eeMem.h"
#include "jsonstring.h"
#include "Nextion.h"
#include <TimeLib.h>

extern Nextion nex;

void TempArray::add()
{
  int iPos = hour() << 2; // 4 per hour
  if( minute() ) // force 1 and 3 slots
  {
    if(minute() < 30)
      iPos ++;
    else if(minute() == 30)
      iPos += 2;
    else
      iPos += 3;
  }
  if(iPos == 0) //fill in to 24:00
  {
    if(m_log[LOG_CNT-3].state == 2)
      memset(&m_log[LOG_CNT-3], 0, sizeof(tempArr));
    m_log[LOG_CNT-2].min = 24*60;
    m_log[LOG_CNT-2].temp = display.m_currentTemp;
    m_log[LOG_CNT-2].state = display.m_bHeater;
    m_log[LOG_CNT-2].rm = display.m_roomTemp;
    m_log[LOG_CNT-2].rh = display.m_rh;
  }
  m_log[iPos].min = (hour() * 60) + minute();
  m_log[iPos].temp = display.m_currentTemp;
  m_log[iPos].state = display.m_bHeater;
  m_log[iPos].rm = display.m_roomTemp;
  m_log[iPos].rh = display.m_rh;

  if(iPos)
    if(m_log[iPos-1].state == 2)
      memset(&m_log[iPos-1], 0, sizeof(tempArr));

  m_log[iPos+1].temp = display.m_currentTemp;
  m_log[iPos+1].state = 2;  // use 2 as a break between old and new
}

String TempArray::get()
{
  String s = "tdata;{\"temp\":[";
  bool bSent = false;

  for (int ent = 0; ent < LOG_CNT-1; ++ent)
  {
    if(m_log[ent].temp)
    {
      if(bSent) s += ",";
      if(m_log[ent].state == 2) // now
      {
        m_log[ent].min = (hour()*60) + minute();
        m_log[ent].temp = display.m_currentTemp;
        m_log[ent].rm = display.m_roomTemp;
        m_log[ent].rh = display.m_rh;
      }
      s += "[";
      s += m_log[ent].min;
      s += ",\""; s += String((float)m_log[ent].temp/10,1); s += "\",";
      s += m_log[ent].state;
      s += ",\""; s += String((float)m_log[ent].rm/10, 1); s += "\"";
      s += ",\""; s += String((float)m_log[ent].rh/10, 1); s += "\"";
      s += "]";

      bSent = true;
    }
  }
  s += "]}";
  return s;
}

#define Sch_Left     30
#define Sch_Top      14
#define Sch_Width   199
#define Sch_Height  99
uint16_t mn, mx;

int16_t TempArray::t2y(uint16_t t) // temp to y position
{
  return Sch_Height - ((t-mn) * Sch_Height / (mx-mn));
}

uint16_t TempArray::tm2x(uint16_t t) // time to x position
{
  return t * Sch_Width / (60*24);
}

uint16_t TempArray::tween(uint16_t t1, uint16_t t2, int m, int r)
{
  uint16_t t = (t2 - t1) * (m * 100 / r ) / 100;
  return t + t1;
}

void TempArray::draw()
{
  mn = 1000; // get range
  mx = 0;
  for(uint8_t i = 0; i < ee.schedCnt[display.m_season]; i++)
  {
    if(mn > ee.schedule[display.m_season][i].setTemp) mn = ee.schedule[display.m_season][i].setTemp;
    if(mx < ee.schedule[display.m_season][i].setTemp) mx = ee.schedule[display.m_season][i].setTemp;
  }
  mn /= 10; mn *= 10; // floor
  mx += (10-(mx%10)); // ciel
  nex.itemText(25, String(mx / 10) );
  nex.itemText(26, String(mn / 10) );

  uint16_t m = Sch_Width - tm2x(ee.schedule[ee.schedCnt[display.m_season]-1][0].timeSch); // wrap line
  uint16_t r = m + tm2x(ee.schedule[display.m_season][0].timeSch);
  uint16_t ttl = tween(ee.schedule[display.m_season][ee.schedCnt[display.m_season]-1].setTemp, ee.schedule[display.m_season][0].setTemp, m, r); // get y of midnight

  uint16_t x = Sch_Left, x2;
  uint16_t y = t2y(ttl) + Sch_Top, y2;

  for(uint8_t i = 0; i < ee.schedCnt[display.m_season]; i++)
  {
    x2 = tm2x(ee.schedule[display.m_season][i].timeSch) + Sch_Left;
    y2 = t2y(ee.schedule[display.m_season][i].setTemp) + Sch_Top;
    nex.line(x, y, x2, y2, rgb16(31, 31, 0) );
    delay(1);
    x = x2;
    y = y2;
  }
  nex.line(x, y, Sch_Left + Sch_Width, t2y(ttl) + Sch_Top, rgb16(31, 31, 0) );
  x = tm2x( hour() * 60 + minute() ) + Sch_Left;
  y = t2y(display.m_currentTemp) + Sch_Top;
  nex.line(x, y-1, x, y+1, rgb16(0, 63, 0) );
}
