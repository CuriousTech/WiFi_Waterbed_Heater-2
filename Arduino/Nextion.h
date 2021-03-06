#ifndef NEXTION_H
#define NEXTION_H
#include <arduino.h>

// from 8 bit components to 5-6-5 bits
#define rgb(r,g,b) ( (((uint16_t)r << 8) & 0xF800) | (((uint16_t)g << 3) & 0x07E0) | ((uint16_t)b >> 3) )
// from 5-6-5 to 16 bit value (max 31, 63, 31)
#define rgb16(r,g,b) ( ((uint16_t)r << 11) | ((uint16_t)g << 5) | (uint16_t)b )

enum Page
{
  Page_Main,
  Page_Thermostat,
  Page_Schedule,
  Page_SSID,
  Page_Keyboard,
  Page_Alarms,
  Page_Clock,
};

class Nextion
{
public:
  Nextion(){};
  int service(char *pBuff);
  void itemText(uint8_t id, String t);
  void btnText(uint8_t id, String t);
  void itemFp(uint8_t id, uint16_t val);
  void refreshItem(String id);
  void fill(uint16_t x, uint16_t y, uint16_t w, uint16_t h, uint16_t color);
  void line(uint16_t x, uint16_t y, uint16_t x2, uint16_t y2, uint16_t color);
  void text(uint16_t x, uint16_t y, uint16_t xCenter, uint16_t color, String sText);
  void visible(String id, uint8_t on);
  void itemPic(uint8_t id, uint8_t idx);
  void itemNum(uint8_t item, int16_t num);
  uint8_t brightness(uint8_t level);
  void setPage(uint8_t n);
  uint8_t getPage(void);
  void gauge(uint8_t id, uint16_t angle);
  void backColor(String sPageName, uint16_t color);
  void itemColor(String s, uint16_t color);
  void cls(uint16_t color);
  void add(uint8_t comp, uint8_t ch, uint16_t val);
  void refresh(bool bOn);
  void getVal(uint8_t item);
  void setVal(uint8_t item, int16_t val);
  void reset(void);
  void sleep(bool bOn);
  void autoWake(bool bOn);
  void FFF(void);
  void checkItem(uint8_t id, uint16_t v);

  int8_t m_valItem = -1;
private:
  void dimmer(void);

  int8_t m_newBrightness = 99;
  int8_t m_brightness = 99;
  uint8_t m_page;
};

//extern Nextion nex;

#endif // NEXTION_H
