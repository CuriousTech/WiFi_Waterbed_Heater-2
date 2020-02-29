#ifndef TEMPARRAY_H
#define TEMPARRAY_H

struct tempArr{
  uint16_t min;
  uint16_t temp;
  uint8_t state;
  uint16_t rm;
  uint16_t rh;
};

#define LOG_CNT 98

void addLog(void);
String getTempArray(void);
void drawSched(void);

class TempArray
{
public:
  TempArray(){}
  void add(void);
  String get(void);
  void draw(void);
protected:
  String jsEntry(uint16_t ent);
  int16_t t2y(uint16_t t);
  uint16_t tm2x(uint16_t t);
  uint16_t tween(uint16_t t1, uint16_t t2, int m, int r);
  tempArr m_log[LOG_CNT];
};

#endif
