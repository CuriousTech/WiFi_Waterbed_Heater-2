#ifndef DISPLAY_H
#define DISPLAY_H

#include <Arduino.h>
#include <ESPAsyncWebServer.h> // https://github.com/me-no-dev/ESPAsyncWebServer

#define NEX_TIMEOUT  90  // 90 seconds
#define NEX_BRIGHT   80  // 100% = full brightness
#define NEX_MEDIUM   10  //
#define NEX_DIM       1  // very dim
#define NEX_OFF       0  // off

struct Notif
{
  uint32_t ip;
  String s;
};

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
  void Notification(String s, IPAddress ip);
  void NotificationCancel(String s);

  uint16_t m_roomTemp;
  uint16_t m_rh;
  uint16_t m_outTemp;
  uint16_t m_outRh;
  int m_currentTemp = 900;
  int m_hiTemp; // current target
  int m_loTemp;
  bool m_bHeater;
  bool m_bAlarmOn;
  uint8_t m_schInd;
  uint8_t m_nLightLevel;
  bool m_bLightOn;
  uint8_t m_LightSet;
  uint8_t m_season;
private:
  void buttonRepeat(void);
  void nextAlarm(void);
  void refreshAll(void);
  void refreshClock(void);
  void updateRSSI(void);
  void updateAlarms(void);
  String fmtTime(uint16_t v);
  void updateSchedule(void);
  void selectSched(uint8_t row, uint8_t col);
  void schedUpDown(bool bUp);

  uint16_t m_backlightTimer = NEX_TIMEOUT;
  uint8_t m_btnMode = 0;
  uint8_t m_btnDelay = 0;
  bool  m_bSliderDn = false;
  uint8_t m_almSelect =0;
  uint8_t m_schedRow =0;
  uint8_t m_schedCol = 0;
  uint8_t m_alarmIdx = 0;
  bool m_bNotifVis = false;
  String m_sNotifCurr;
  IPAddress m_notifIP;
#define NOTIFS 10
  Notif m_Notif[NOTIFS];
};

enum reportReason
{
  Reason_Setup,
  Reason_Switch,
  Reason_Level,
  Reason_Motion,
  Reason_Notif,
};

extern Display display;

#endif // DISPLAY_H
