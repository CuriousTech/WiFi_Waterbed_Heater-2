#include "eeMem.h"

#ifdef ESP32
#include <Preferences.h>
Preferences prefs;
#else
#include <EEPROM.h>
#endif

void eeMem::init()
{
#ifdef ESP32
  prefs.begin("my-app", false);
#else
  EEPROM.begin(EESIZE);
#endif
  verify(false);
}

bool eeMem::update(bool bForce) // write the settings if changed
{
  uint16_t old_sum = ee.sum;
  ee.sum = 0;
  ee.sum = Fletcher16((uint8_t*)this + offsetof(eeMem, size), EESIZE);

  if(bForce == false && old_sum == ee.sum)
  {
    return false; // Nothing has changed?
  }

  ee.sum = 0;
  ee.sum = Fletcher16((uint8_t*)this + offsetof(eeMem, size), EESIZE );

  uint8_t *pData = (uint8_t *)this + offsetof(eeMem, size);
#ifdef ESP32
  prefs.putBytes("Config", pData, EESIZE);
#else
  for(int addr = 0; addr < EESIZE; addr++)
  {
    EEPROM.write(addr, pData[addr] );
  }
  EEPROM.commit();
#endif
  return true;
}

bool eeMem::verify(bool bComp)
{
  uint8_t data[EESIZE];
  uint16_t *pwTemp = (uint16_t *)data;

#ifdef ESP32
  prefs.getBytes("Config", data, EESIZE);
#else
  for(int addr = 0; addr < EESIZE; addr++)
  {
    data[addr] = EEPROM.read( addr );
  }
#endif
  if(pwTemp[0] != EESIZE)
    return false; // revert to defaults if struct size changes

  uint16_t sum = pwTemp[1];
  pwTemp[1] = 0;
  pwTemp[1] = Fletcher16(data, EESIZE );
  if(pwTemp[1] != sum)
    return false; // revert to defaults if sum fails

  if(bComp)
    return ( !memcmp(this + offsetof(eeMem, size), data, EESIZE ) ); // don't load
  memcpy(this + offsetof(eeMem, size), data, EESIZE );
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
