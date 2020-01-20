#ifndef DISPLAY_H
#define DISPLAY_H

#include <Arduino.h>

#define NEX_TIMEOUT  90  // 90 seconds
#define NEX_BRIGHT   80  // 100% = full brightness
#define NEX_MEDIUM   10  //
#define NEX_DIM       1  // very dim
#define NEX_OFF       0  // off

class Display
{
public:
  Display(){}
  void init(void);
  void oneSec(void);
  bool screen(bool bOn);
  void reset(void);
  bool checkNextion(void); // all the Nextion recieved commands
  bool isOff(void);
  void updateLevel(uint8_t lvl);
  bool checkAlarms(void);
  uint16_t m_roomTemp;
  uint16_t m_rh;
  int m_currentTemp = 900;
  int m_hiTemp; // current target
  int m_loTemp;
  bool m_bHeater;
  bool m_bAlarmOn;
  uint8_t m_schInd;
  uint8_t m_nLightLevel;
  bool m_bLightOn;
  uint8_t m_LightSet;
private:
  void buttonRepeat(void);
  void refreshAll(void);
  void updateRSSI(void);
  void updateAlarms(void);
  String fmtTime(uint16_t v);
  uint16_t m_backlightTimer = NEX_TIMEOUT;
  uint8_t m_btnMode;
  uint8_t m_btnDelay;
  bool  m_bSliderDn;
  uint8_t m_almSelect;
};

enum reportReason
{
  Reason_Setup,
  Reason_Switch,
  Reason_Level,
  Reason_Motion,
  Reason_Test,
};

#endif // DISPLAY_H
