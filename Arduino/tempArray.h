
struct tempArr{
  uint16_t min;
  uint16_t temp;
  uint8_t state;
  uint16_t rm;
  uint16_t rh;
};
#define LOG_CNT 98
extern tempArr tempArray[LOG_CNT];

void addLog(void);
String jsEntry(uint16_t ent);
void drawSched(void);
