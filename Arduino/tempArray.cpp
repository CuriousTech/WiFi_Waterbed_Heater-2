#include "display.h"
#include "tempArray.h"
#include "eeMem.h"
#include "jsonstring.h"
#include "Nextion.h"
#include <TimeLib.h>

tempArr tempArray[LOG_CNT];

extern Nextion nex;

void addLog()
{
  int iPos = hour() << 2; //+ ( minute() / 15); // 4 per hour
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
    if(tempArray[LOG_CNT-3].state == 2)
      memset(&tempArray[LOG_CNT-3], 0, sizeof(tempArr));
    tempArray[LOG_CNT-2].min = 24*60;
    tempArray[LOG_CNT-2].temp = display.m_currentTemp;
    tempArray[LOG_CNT-2].state = display.m_bHeater;
    tempArray[LOG_CNT-2].rm = display.m_roomTemp;
    tempArray[LOG_CNT-2].rh = display.m_rh;
  }
  tempArray[iPos].min = (hour() * 60) + minute();
  tempArray[iPos].temp = display.m_currentTemp;
  tempArray[iPos].state = display.m_bHeater;
  tempArray[iPos].rm = display.m_roomTemp;
  tempArray[iPos].rh = display.m_rh;

  if(iPos)
    if(tempArray[iPos-1].state == 2)
      memset(&tempArray[iPos-1], 0, sizeof(tempArr));

  tempArray[iPos+1].temp = display.m_currentTemp;
  tempArray[iPos+1].state = 2;  // use 2 as a break between old and new
}

String jsEntry(uint16_t ent)
{
  jsonString js;

  if(tempArray[ent].state == 2) // now
  {
    tempArray[ent].min = (hour()*60) + minute();
    tempArray[ent].temp = display.m_currentTemp;
    tempArray[ent].rm = display.m_roomTemp;
    tempArray[ent].rh = display.m_rh;
  }
  if(tempArray[ent].temp) // only send entries in use
  {
    js.Var("tm", tempArray[ent].min );
    js.Var("t", String((float)tempArray[ent].temp/10,1) );
    js.Var("s", tempArray[ent].state );
    js.Var("rm", String((float)tempArray[ent].rm/10, 1) );
    js.Var("rh", String((float)tempArray[ent].rh/10, 1) );
  }
  return js.Close();
}

#define Sch_Left     30
#define Sch_Top      14
#define Sch_Width   199
#define Sch_Height  99
uint16_t mn, mx;

int16_t t2y(uint16_t t) // temp to y position
{
  return Sch_Height - ((t-mn) * Sch_Height / (mx-mn));
}

uint16_t tm2x(uint16_t t) // time to x position
{
  return t * Sch_Width / (60*24);
}

uint16_t tween(uint16_t t1, uint16_t t2, int m, int r)
{
  uint16_t t = (t2 - t1) * (m * 100 / r ) / 100;
  return t + t1;
}

void drawSched()
{
  nex.refreshItem(2);
  mn = 1000; // get range
  mx = 0;
  for(uint8_t i = 0; i < ee.schedCnt; i++)
  {
    if(mn > ee.schedule[i].setTemp) mn = ee.schedule[i].setTemp;
    if(mx < ee.schedule[i].setTemp) mx = ee.schedule[i].setTemp;
  }
  mn /= 10; mn *= 10; // floor
  mx += (10-(mx%10)); // ciel
  nex.itemText(21, String(mx / 10) );
  nex.itemText(20, String(mn / 10) );

  uint16_t m = Sch_Width - tm2x(ee.schedule[ee.schedCnt-1].timeSch); // wrap line
  uint16_t r = m + tm2x(ee.schedule[0].timeSch);
  uint16_t ttl = tween(ee.schedule[ee.schedCnt-1].setTemp, ee.schedule[0].setTemp, m, r); // get y of midnight

  uint16_t x = Sch_Left, x2;
  uint16_t y = t2y(ttl) + Sch_Top, y2;

  for(uint8_t i = 0; i < ee.schedCnt; i++)
  {
    x2 = tm2x(ee.schedule[i].timeSch) + Sch_Left;
    y2 = t2y(ee.schedule[i].setTemp) + Sch_Top;
    nex.line(x, y, x2, y2, rgb16(31, 31, 0) );
    delay(1);
    x = x2;
    y = y2;
  }
  nex.line(x, y, Sch_Left + Sch_Width, t2y(ttl) + Sch_Top, rgb16(31, 31, 0) );
}
