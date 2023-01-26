#ifndef EEMEM_H
#define EEMEM_H

#include <Arduino.h>

struct Sched
{
  uint16_t setTemp;
  uint16_t timeSch;
  uint8_t thresh;
  uint8_t wday;  // Todo: weekday 0=any, 1-7 = day of week
};

struct Alarm
{
  uint16_t ms;  // 0 = inactive
  uint16_t freq;
  uint16_t timeSch;
  uint8_t  wday;  // Todo: weekday 0=any, bits 0-6 = day of week
};

#define MAX_SCHED 8

#define EESIZE (offsetof(eeMem, end) - offsetof(eeMem, size) )

class eeMem
{
public:
  eeMem();
  bool update(bool bForce);
  bool verify(bool bComp);
private:
  uint16_t Fletcher16( uint8_t* data, int count);
public:
  uint16_t size = EESIZE;          // if size changes, use defauls
  uint16_t sum = 0xAAAA;           // if sum is diiferent from memory struct, write
  char     szSSID[32] = "";
  char     szSSIDPassword[64] = "";
  uint16_t vacaTemp = 700;     // vacation temp
  int8_t  tz = -5;            // Timezone offset from your global server
  uint8_t schedCnt[2] = {5,5};   // number of active scedules
  bool    bVaca = false;         // vacation enabled
  bool    bAvg = true;          // average target between schedules
  bool    bEco = false;          // eco mode
  Sched   schedule[2][MAX_SCHED] =  // 2x22x8 bytes
  {
    {
      {831,  3*60, 3, 0},
      {824,  6*60, 2, 0},  // temp, time, thresh, wday
      {819,  8*60, 3, 0},
      {819, 16*60, 3, 0},
      {834, 21*60, 3, 0},
      {830,  0*60, 3, 0},
      {830,  0*60, 3, 0},
      {830,  0*60, 3, 0}
    },
    {
      {831,  3*60, 3, 0},
      {824,  6*60, 2, 0},  // temp, time, thresh, wday
      {819,  8*60, 3, 0},
      {819, 16*60, 3, 0},
      {834, 21*60, 3, 0},
      {830,  0*60, 3, 0},
      {830,  0*60, 3, 0},
      {830,  0*60, 3, 0}
    },
  };
  uint16_t ppkwh = 164; // $0.164 / KWH
  uint16_t rate = 50; // seconds
  uint16_t watts = 290; // Heating pad
  uint16_t ppkwm[12] = {153,153,153,125,125,125,125,146,146,153,154,154}; // ppkwh per month
  uint32_t tSecsMon[12] = {1254411,1067144,916519,850686,122453,268488,302535,396531,501161,552347,427980,883172}; // total secwatt hours per month (copied from page)
  int16_t  tAdj[2] = {0,0};
  int16_t  pids[3] = {60*3,60*1, 5}; // Todo: real PID
  uint8_t  hostIP[4] = {192,168,31,100};
  uint16_t hostPort = 80;
  uint8_t  hvacIP[4] = {192,168,31,46};
  uint16_t hvacPort = 80;
  uint8_t  lightIP[2][4] = {{192,168,31,8}, {0,0,0,0}};
  uint8_t  resIP[4]; // reserved for later
  uint16_t resPort = 80;
  uint32_t nOvershootTime;
  int16_t  nOvershootTempDiff;
  Alarm   alarm[MAX_SCHED] = 
  { // alarms
    {0, 1000, 8*60, 0x3E},
    {0},
  };
  uint8_t end;
}; // 392

extern eeMem ee;
#endif // EEMEM_H
