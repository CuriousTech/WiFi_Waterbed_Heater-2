#include "eeMem.h"
#include <EEPROM.h>

extern void eSend(String s);

eeSet ee = {
  sizeof(eeSet), 0xAAAA,
  "",  // saved SSID
  "", // router password
  700, -5, // vacaTemp, TZ
  5,       // schedCnt
  false,  // vacation mode
  true,  // average
  false, // Eco mode
  {
    {831,  3*60, 3, 0, "Midnight"},
    {824,  6*60, 2, 0, "Early"},  // temp, time, thresh, wday
    {819,  8*60, 3, 0, "Morning"},
    {819, 16*60, 3, 0, "Day"},
    {834, 21*60, 3, 0, "Night"},
    {830,  0*60, 3, 0, "Sch6"},
    {830,  0*60, 3, 0, "Sch7"},
    {830,  0*60, 3, 0, "Sch8"}
  },
  140, // ppkwh (0.140)
  50, // rate
  290, // watts
  {140,140,140,140,140,140,140,140,140,140,140,140}, // ppkwh per month
  {140797,90131,189843,161339,122453,33761,32020,0,0,32637,133370,204380}, // seconds
  {0,0}, // tAdj[2]
  {60*3,60*1, 5}, // pids
  {192,168,31,100}, // host
  80,
  {192,168,31,125}, // thermostat
  80,
  {{192,168,31,8},{0,0,0,0}}, // lights
  {0,0,0,0}, // extra
  80,
  {0}, // res
  { // alarms
    {0, 1000, 8*60, 0x3E},
    {0},
  }, // alarms
};

eeMem::eeMem()
{
  EEPROM.begin(sizeof(eeSet));
  verify(false);
}

bool eeMem::update(bool bForce) // write the settings if changed
{
  uint16_t old_sum = ee.sum;
  ee.sum = 0;
  ee.sum = Fletcher16((uint8_t*)&ee, sizeof(eeSet));

  if(bForce == false && old_sum == ee.sum)
  {
    return false; // Nothing has changed?
  }

  ee.sum = 0;
  ee.sum = Fletcher16((uint8_t*)&ee, sizeof(eeSet) );

  uint8_t *pData = (uint8_t *)&ee;
  for(int addr = 0; addr < sizeof(eeSet); addr++)
  {
    EEPROM.write(addr, pData[addr] );
  }
  EEPROM.commit();
  return true;
}

bool eeMem::verify(bool bComp)
{
  uint8_t data[sizeof(eeSet)];
  uint16_t *pwTemp = (uint16_t *)data;

  for(int addr = 0; addr < sizeof(eeSet); addr++)
  {
    data[addr] = EEPROM.read( addr );
  }
  if(pwTemp[0] != sizeof(eeSet))
    return false; // revert to defaults if struct size changes
  uint16_t sum = pwTemp[1];
  pwTemp[1] = 0;
  pwTemp[1] = Fletcher16(data, sizeof(eeSet) );
  if(pwTemp[1] != sum) return false; // revert to defaults if sum fails

  if(bComp)
    return ( !memcmp(&ee, data, sizeof(eeSet) ) ); // don't load
  memcpy(&ee, data, sizeof(eeSet) );
  return true;
}

uint16_t eeMem::Fletcher16( uint8_t* data, int count)
{
   uint16_t sum1 = 0;
   uint16_t sum2 = 0;

   for( int index = 0; index < count; ++index )
   {
      sum1 = (sum1 + data[index]) % 255;
      sum2 = (sum2 + sum1) % 255;
   }
   return (sum2 << 8) | sum1;
}
