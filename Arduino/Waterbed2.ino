/**The MIT License (MIT)

Copyright (c) 2020 by Greg Cunningham, CuriousTech

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
*/

// #define SDEBUG
//uncomment to enable Arduino IDE Over The Air update code
#define OTA_ENABLE
//#define USE_SPIFFS

#include <Wire.h>
#include <EEPROM.h>
#include <ESP8266mDNS.h>
#include "WiFiManager.h"
#include <ESPAsyncWebServer.h> // https://github.com/me-no-dev/ESPAsyncWebServer
#ifdef OTA_ENABLE
#include <FS.h>
#include <ArduinoOTA.h>
#endif
#ifdef USE_SPIFFS
#include <FS.h>
#include <SPIFFSEditor.h>
#else
#include "pages.h"
#endif
#include <OneWire.h>
#include <TimeLib.h> // http://www.pjrc.com/teensy/td_libs_Time.html
#include <UdpTime.h> // https://github.com/CuriousTech/ESP07_WiFiGarageDoor/tree/master/libraries/UdpTime
#include "eeMem.h"
#include "RunningMedian.h"
#include <JsonParse.h> // https://github.com/CuriousTech/ESP8266-HVAC/tree/master/Libraries/JsonParse
#include <JsonClient.h>
#include <SHT21.h> // https://github.com/CuriousTech/ESP8266-HVAC/tree/master/Libraries/SHT21
#include "jsonstring.h"
#include "display.h"
#include "tempArray.h"

const char controlPassword[] = "password";    // device password for modifying any settings
const int serverPort = 80;                    // HTTP port
const char hostName[] = "Waterbed";

#define BTN      0 //  top
#define ENC_A    2
#define ESP_LED  2  //Blue LED on ESP12 (on low)
#define SDA      4
#define SCL      5
#define HEAT     12  // Heater output
#define DS18B20  13
#define MOTION   14  // PIR sensor
#define TONE     15  // Speaker.  Beeps on powerup, but can also be controlled by aother IoT or add an alarm clock.
#define ENC_B    16

TempArray ta;

OneWire ds(DS18B20);
byte ds_addr[8];
IPAddress lastIP;
int nWrongPass;

SHT21 sht(SDA, SCL, 4);
uint16_t light;

Display display;
eeMem eemem;

WiFiManager wifi;  // AP page:  192.168.4.1
AsyncWebServer server( serverPort );
AsyncWebSocket ws("/ws"); // access at ws://[esp ip]/ws

UdpTime utime;

bool bCF = false;

uint32_t onCounter;
bool bMotion = true;

int toneFreq = 1000;
int tonePeriod = 100;
uint32_t toneEnd;
uint8_t nAlarming;
uint16_t nSnoozeTimer;

void jsonCallback(int16_t iEvent, uint16_t iName, int iValue, char *psValue);
JsonParse jsonParse(jsonCallback);
void jsonPushCallback(int16_t iEvent, uint16_t iName, int iValue, char *psValue);
JsonClient jsonPush(jsonPushCallback);

const char days[7][4] = {"Sun","Mon","Tue","Wed","Thr","Fri","Sat"};
const char months[12][4] = {"Jan","Feb","Mar","Apr","May","Jun","Jul","Aug","Sep","Oct","Nov","Dec"};

bool updateAll(bool bForce)
{
  return eemem.update(bForce);
}

String dataJson()
{
  jsonString js("state");

  js.Var("t", (uint32_t)(now() - ((ee.tz + utime.getDST()) * 3600)) );
  js.Var("waterTemp", String((float)display.m_currentTemp/10, 1) );
  js.Var("setTemp", String((float)ee.schedule[display.m_schInd].setTemp / 10, 1) );
  js.Var("hiTemp",  String((float)display.m_hiTemp/10, 1) );
  js.Var("loTemp",  String((float)display.m_loTemp/10, 1) );
  js.Var("on",   digitalRead(HEAT));
  js.Var("temp", String((float)display.m_roomTemp/10, 1) );
  js.Var("rh",   String((float)display.m_rh/10, 1) );
  js.Var("c",    String((bCF) ? "C":"F"));
  js.Var("oc",   onCounter );
  js.Var("mot",  bMotion);
  return js.Close();
}

String setJson() // settings
{
  jsonString js("set");

  js.Var("vt", String((float)ee.vacaTemp/10, 1) );
  js.Var("o",   0);
  js.Var("tz",  ee.tz);
  js.Var("avg", ee.bAvg);
  js.Var("ppkwh",ee.ppkwh);
  js.Var("vo",  ee.bVaca);
  js.Var("idx", display.m_schInd);
  js.Var("cnt", ee.schedCnt);
  js.Var("w",  ee.watts);
  js.Var("r",  ee.rate);
  js.Var("e",  ee.bEco);
  js.Array("item", ee.schedule, 8);
  js.Array("ts", ee.tSecsMon, 12);
  js.Array("ppkwm", ee.ppkwm, 12);
  return js.Close();
}

void ePrint(const char *s)
{
  ws.textAll(String("print;") + s);
}

void parseParams(AsyncWebServerRequest *request)
{
  static char temp[256];
  char password[64];
  int16_t val;

 if(request->params() == 0)
    return;

  // get password first
  for ( uint8_t i = 0; i < request->params(); i++ ) {
    AsyncWebParameter* p = request->getParam(i);
    p->value().toCharArray(temp, 100);
    String s = wifi.urldecode(temp);
    if( p->name().equals("key") )
    {
        s.toCharArray(password, sizeof(password));
        break;
    }
  }

  IPAddress ip = request->client()->remoteIP();

  if(strcmp(controlPassword, password) || nWrongPass)
  {
    if(nWrongPass == 0)
      nWrongPass = 10;
    else if((nWrongPass & 0xFFFFF000) == 0 ) // time doubles for every high speed wrong password attempt.  Max 1 hour
      nWrongPass <<= 1;
    if(ip != lastIP)  // if different IP drop it down
       nWrongPass = 10;

    jsonString js("hack");
    js.Var("ip", request->client()->remoteIP().toString() );
    js.Var("pass", password);
    ws.textAll(js.Close());
    lastIP = ip;
    return;
  }

  lastIP = ip;

  const char Names[][10]={
    "rate", // 0
    "reset",
    "beep",
    "ssid",
    "pass",
    "cost",
    "watts",
    "vibe",
    "tadj",
    "ppkwh",
    "hostip",
    "lightip", // 11
  };

  for ( uint8_t i = 0; i < request->params(); i++ ) {
    AsyncWebParameter* p = request->getParam(i);
    p->value().toCharArray(temp, 100);
    String s = wifi.urldecode(temp);

    int which = p->name().charAt(1) - '0'; // limitation = 9
    if(which >= MAX_SCHED) which = MAX_SCHED - 1; // safety
    if(which < 0) which = 0; // safety
    val = s.toInt();

    uint8_t idx;
    for(idx = 0; Names[idx][0]; idx++)
      if( p->name().equals(Names[idx]) )
        break;
    switch( idx )
    {
      case 0: // rate
        if(val == 0)
          break; // don't allow 0
        ee.rate = val;
        sendState();
        break;
      case 1: // reset
        updateAll(true);
        delay(100);
        ESP.reset();
        break;
      case 2: // beep : /s?key=pass&beep=1000,800 (tone or ms,tone)
        {
          int n = s.indexOf(",");
          if(n >0)
          {
            tonePeriod = constrain(val, 10, 1000);
            s.remove(0, n+1);
            val = s.toInt();
          }
          toneFreq = val;
        }
        break;
      case 3: // SSID
        s.toCharArray(ee.szSSID, sizeof(ee.szSSID));
        break;
      case 4: // pass
        s.toCharArray(ee.szSSIDPassword, sizeof(ee.szSSIDPassword));
        updateAll( false );
        wifi.setPass();
        break;
      case 5: // restore the month's total (in cents) after updates
        break;
      case 6: // restore the month's total (in watthours) after updates
        break;
#ifdef VIBE
      case 7: // vibe
        {
          int n = s.indexOf(",");
          if(n >0)
          {
            s.remove(0, n+1);
            vibeCnt = s.toInt();
          }
        }
        vibePeriod = val;
        pinMode(VIBE, OUTPUT);
        digitalWrite(VIBE, LOW);
        break;
#endif
      case 8: // tadj (for room temp)
        ee.tAdj[1] = constrain(val, -100, 80);
        break;
      case 9: // ppkwh
        ee.ppkwm[month()-1] = ee.ppkwh = val;
        break;
      case 10: // host IP / port  (call from host with ?h=80)
        ee.hostIP[0] = ip[0];
        ee.hostIP[1] = ip[1];
        ee.hostIP[2] = ip[2];
        ee.hostIP[3] = ip[3];
        ee.hostPort = val ? val:80;
        break;
      case 11: // lightIP1
        ip.fromString(s.c_str());
        ee.lightIP[0][0] = ip[0];
        ee.lightIP[0][1] = ip[1];
        ee.lightIP[0][2] = ip[2];
        ee.lightIP[0][3] = ip[3];
        break;
    }
  }
}

void handleS(AsyncWebServerRequest *request) { // standard params, but no page
  parseParams(request);

  jsonString js;
  String s = WiFi.localIP().toString() + ":";
  s += serverPort;
  js.Var("ip", s);
  request->send ( 200, "text/json", js.Close() );
}

const char *jsonListCmd[] = { "cmd",
  "key",
  "oled",
  "TZ",
  "avg",
  "cnt",
  "tadj", // 5
  "ppkwh",
  "vaca",
  "vacatemp",
  "I",
  "N", // 10
  "S",
  "T",
  "H",
  "beepF",
  "beepP", // 15
  "vibe",
  "watts",
  "dot",
  "save",
  "aadj", // 20
  "eco",
  "outtemp",
  "outrh",
  NULL
};

bool bKeyGood;

void jsonCallback(int16_t iEvent, uint16_t iName, int iValue, char *psValue)
{
  if(!bKeyGood && iName) return; // only allow for key
  char *p, *p2;
  static int beepPer = 1000;
  static int item = 0;

  switch(iEvent)
  {
    case 0: // cmd
      switch(iName)
      {
        case 0: // key
          if(!strcmp(psValue, controlPassword)) // first item must be key
            bKeyGood = true;
          break;
        case 1: // OLED
//          ee.bEnableOLED = iValue ? true:false;
          break;
        case 2: // TZ
          ee.tz = iValue;
          utime.start();
          break;
        case 3: // avg
          ee.bAvg = iValue;
          break;
        case 4: // cnt
          ee.schedCnt = constrain(iValue, 1, 8);
          ws.textAll(setJson()); // update all the entries
          break;
        case 5: // tadj
          changeTemp(iValue, false);
          ws.textAll(setJson()); // update all the entries
          break;
        case 6: // ppkw
          ee.ppkwm[month()-1] = ee.ppkwh = iValue;
          break;
        case 7: // vaca
          ee.bVaca = iValue;
          break;
        case 8: // vacatemp
          if(bCF)
            ee.vacaTemp = constrain( (int)(atof(psValue)*10), 100, 290); // 10-29C
          else
            ee.vacaTemp = constrain( (int)(atof(psValue)*10), 600, 840); // 60-84F
          break;
        case 9: // I
          item = iValue;
          break;
        case 10: // N
          strncpy(ee.schedule[item].name, psValue, sizeof(ee.schedule[0].name) );
          break;
        case 11: // S
          p = strtok(psValue, ":");
          p2 = strtok(NULL, "");
          if(p && p2){
            iValue *= 60;
            iValue += atoi(p2);
          }
          ee.schedule[item].timeSch = iValue;
          break;
        case 12: // T
          ee.schedule[item].setTemp = atof(psValue)*10;
          break;
        case 13: // H
          ee.schedule[item].thresh = (int)(atof(psValue)*10);
          checkLimits();      // constrain and check new values
          checkSched(true);   // reconfigure to new schedule
          break;
       case 14: // beepF
          toneFreq = iValue;
          break;
       case 15: // beepP (beep period)
          tonePeriod = iValue;
          break;
       case 16: // vibe (vibe period)
#ifdef VIBE
          vibePeriod = iValue;
          vibeCnt = 1;
          pinMode(VIBE, OUTPUT);
          digitalWrite(VIBE, LOW);
#endif
          break;
        case 17: // watts
          ee.watts = iValue;
          break;
        case 18: // dot displayOnTimer
          if(iValue)
            display.screen(true);
          break;
        case 19: // save
          updateAll(true);
          break;
        case 20: // aadj
          changeTemp(iValue, true);
          ws.textAll(setJson()); // update all the entries
          break;
        case 21: // eco
          ee.bEco = iValue ? true:false;
          setHeat();
          break;
        case 22: // outtemp
          display.m_outTemp = iValue;
          break;
        case 23: // outrh
          display.m_outRh = iValue;
          break;
      }
      break;
  }
}

const char *jsonListPush[] = { "time",
  "time", // 0
  "ppkw",
  NULL
};

const char *jsonListPush2[] = { "state",
  "on", // 0
  "lvl",
  NULL
};

void jsonPushCallback(int16_t iEvent, uint16_t iName, int iValue, char *psValue)
{
  switch(iEvent)
  {
    case -1: // status
      break;
    case 0: // time
      switch(iName)
      {
        case 0: // time
          setTime(iValue + ( (ee.tz + utime.getDST() ) * 3600));
          break;
        case 1: // ppkw
          ee.ppkwm[month()-1] = ee.ppkwh = iValue;
          break;
      }
      break;
    case 1: // state (from lights)
      switch(iName)
      {
        case 0: // on
          display.m_bLightOn = iValue ? true:false;
          break;
        case 1: // lvl
          display.updateLevel(iValue);
          break;
      }
      break;
  }
}

struct cQ
{
  IPAddress ip;
  String sUri;
  uint16_t port;
};
#define CQ_CNT 16
cQ queue[CQ_CNT];

void checkQueue()
{
  int i;
  for(i = 0; i < CQ_CNT; i++)
  {
    if(queue[i].ip[0])
      break;
  }
  if(i == CQ_CNT) return; // nothing to do

  if(jsonPush.status() != JC_IDLE) // These should be fast, so kill if not
    return;

  jsonPush.begin(queue[i].ip.toString().c_str(), queue[i].sUri.c_str(), queue[i].port, false, false, NULL, NULL, 300);
  jsonPush.addList(jsonListPush);
  jsonPush.addList(jsonListPush2);
  queue[i].ip[0] = 0;
}

void callQueue(IPAddress ip, String sUri, uint16_t port)
{
  int i;
  for(i = 0; i < CQ_CNT; i++)
  {
    if(queue[i].ip[0] == 0)
      break;
  }
  if(i == CQ_CNT) return; // full
  queue[i].ip = ip;
  queue[i].sUri = sUri;
  queue[i].port = port;
}

void CallHost(reportReason r)
{
  if(ee.hostIP[0] == 0) // no host set
    return;

  String sUri = String("/wifi?name=\"");
  sUri += hostName;
  sUri += "\"&reason=";

  switch(r)
  {
    case Reason_Setup:
      sUri += "setup&port="; sUri += serverPort;
      sUri += "&on=1";
      break;
    case Reason_Switch:
      sUri += "switch&on="; sUri += display.m_bLightOn;
      break;
    case Reason_Level:
      sUri += "level&value="; sUri += display.m_nLightLevel;
      break;
    case Reason_Motion:
      sUri += "motion";
      break;
  }

  IPAddress ip(ee.hostIP);
  callQueue(ip, sUri, ee.hostPort);
}

void LightSwitch(uint8_t t, uint8_t v)
{
  if(ee.lightIP[0][0] == 0)
    return;
  String sUri = "/s?key=esp8266ct&";
  switch(t)
  {
    case 0: // switch
      sUri += "on=";
      break;
    case 1: // level
      sUri += "level=";
      break;
  }
  sUri += v;
  IPAddress ip(ee.lightIP[0]);
  callQueue(ip, sUri, 80);
}

void onWsEvent(AsyncWebSocket * server, AsyncWebSocketClient * client, AwsEventType type, void * arg, uint8_t *data, size_t len)
{  //Handle WebSocket event
  static bool bRestarted = true;
  String s;

  switch(type)
  {
    case WS_EVT_CONNECT:      //client connected
      if(bRestarted)
      {
        bRestarted = false;
        client->text("alert;Restarted");
      }
      s = dataJson();
      client->text(s);
      s = setJson();
      client->text(s);
//      client->ping();
      break;
    case WS_EVT_DISCONNECT:    //client disconnected
      break;
    case WS_EVT_ERROR:    //error was received from the other end
      break;
    case WS_EVT_PONG:    //pong message was received (in response to a ping request maybe)
      break;
    case WS_EVT_DATA:  //data packet
      AwsFrameInfo * info = (AwsFrameInfo*)arg;
      if(info->final && info->index == 0 && info->len == len){
        //the whole message is in a single frame and we got all of it's data
        if(info->opcode == WS_TEXT){
          data[len] = 0;

          char *pCmd = strtok((char *)data, ";"); // assume format is "name;{json:x}"
          char *pData = strtok(NULL, "");
          if(pCmd == NULL || pData == NULL) break;
          bKeyGood = false; // for callback (all commands need a key)
          jsonParse.process(pCmd, pData);
          ws.textAll(setJson()); // update the page settings
        }
      }
      break;
  }
}

void setup()
{
  Serial.begin(115200);
#ifdef SDEBUG
  delay(100);
  Serial.println();
  Serial.println("Starting");
#endif

  pinMode(MOTION, INPUT);
  pinMode(ESP_LED, OUTPUT);
  pinMode(TONE, OUTPUT);
  digitalWrite(TONE, LOW);
  pinMode(BTN, INPUT_PULLUP);
  pinMode(HEAT, OUTPUT);
  digitalWrite(HEAT, LOW);
  digitalWrite(ESP_LED, LOW);

  WiFi.hostname(hostName);
  wifi.autoConnect(hostName, controlPassword); // Tries config AP, then starts softAP mode for config
  if(!wifi.isCfg() == false)
  {
    MDNS.begin ( hostName, WiFi.localIP() );
#ifdef SDEBUG
    Serial.println("");
    Serial.println("WiFi connected");
    Serial.println("IP address: ");
    Serial.println(WiFi.localIP());
#endif
  }

#ifdef USE_SPIFFS
  SPIFFS.begin();
  server.addHandler(new SPIFFSEditor("admin", controlPassword));
#endif

  // attach AsyncWebSocket
  ws.onEvent(onWsEvent);
  server.addHandler(&ws);

  server.on ( "/", HTTP_GET, [](AsyncWebServerRequest *request)
  {
    if(wifi.isCfg())
      request->send( 200, "text/html", wifi.page() ); // WIFI config page
  });
  server.on ( "/iot", HTTP_GET | HTTP_POST, [](AsyncWebServerRequest *request )
  {
    parseParams(request);
    #ifdef USE_SPIFFS
      request->send(SPIFFS, "/index.htm");
    #else
      request->send_P(200, "text/html", index_page);
    #endif
  });
  server.on ( "/s", HTTP_GET | HTTP_POST, handleS );
  server.on ( "/set", HTTP_GET, [](AsyncWebServerRequest *request){
    request->send( 200, "text/json", setJson() );
  });
  server.on ( "/json", HTTP_GET, [](AsyncWebServerRequest *request){
    request->send( 200, "text/json", dataJson() );
  });

  server.onNotFound([](AsyncWebServerRequest *request){ // be silent
//    request->send(404);
  });
  server.on("/heap", HTTP_GET, [](AsyncWebServerRequest *request){
    request->send(200, "text/plain", String(ESP.getFreeHeap()));
  });
  server.on("/scan", HTTP_GET, [](AsyncWebServerRequest *request){
    String json = "[";
    int n = WiFi.scanComplete();
    if(n == -2){
      WiFi.scanNetworks(true);
    } else if(n){
      for (int i = 0; i < n; ++i){
        if(i) json += ",";
        jsonString js;

        js.Var("rssi", String(WiFi.RSSI(i)) );
        js.Var("ssid", WiFi.SSID(i) );
        js.Var("bssid", WiFi.BSSIDstr(i) );
        js.Var("channel", String(WiFi.channel(i)) );
        js.Var("secure", String(WiFi.encryptionType(i)) );
        js.Var("hidden", String(WiFi.isHidden(i)?"true":"false") );
        json += js.Close();
      }
      WiFi.scanDelete();
      if(WiFi.scanComplete() == -2){
        WiFi.scanNetworks(true);
      }
    }
    json += "]";
    request->send(200, "text/json", json);
  });
  server.on("/data", HTTP_GET, [](AsyncWebServerRequest *request){
    request->send(200, "text/json", ta.get() );
  });

  server.on("/favicon.ico", HTTP_GET, [](AsyncWebServerRequest *request){
#ifdef USE_SPIFFS
      request->send(SPIFFS, "/favicon.ico");
#else
    AsyncWebServerResponse *response = request->beginResponse_P(200, "image/x-icon", favicon, sizeof(favicon));
    response->addHeader("Content-Encoding", "gzip");
    request->send(response);
#endif
  });

  server.onFileUpload([](AsyncWebServerRequest *request, String filename, size_t index, uint8_t *data, size_t len, bool final){
  });
  server.onRequestBody([](AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total){
  });
  server.begin();

#ifdef OTA_ENABLE
  ArduinoOTA.setHostname(hostName);
  ArduinoOTA.begin();
  ArduinoOTA.onStart([]() {
    digitalWrite(HEAT, LOW);
    updateAll( true );
  });
#endif
  jsonParse.addList(jsonListCmd);

  if( ds.search(ds_addr) )
  {
 #ifdef SDEBUG
    Serial.print("OneWire device: "); // 28 22 92 29 7 0 0 6B
    for( int i = 0; i < 8; i++) {
      Serial.print(ds_addr[i], HEX);
      Serial.print(" ");
    }
    Serial.println("");
    if( OneWire::crc8( ds_addr, 7) != ds_addr[7]){
      Serial.println("Invalid CRC");
    }
 #endif
  }
  else
  {
#ifdef SDEBUG
    Serial.println("No OneWire devices");
 #endif
  }

  if(wifi.isCfg() == false)
  {
    utime.start();
    MDNS.addService("iot", "tcp", serverPort);
  }

  sht.init();
  display.init();
  digitalWrite(ESP_LED, HIGH);
  Tone(2000, 50);
  Tone(5000, 100);
}

uint8_t ssCnt = 30;
uint8_t motCnt;
 
void loop()
{
  static RunningMedian<uint16_t,24> tempMedian[2];
  static uint8_t min_save, sec_save, mon_save = 0;
  static bool bLastOn;

  MDNS.update();
#ifdef OTA_ENABLE
  ArduinoOTA.handle();
#endif
  checkButtons();
  checkQueue();
  if(display.checkNextion())  // check for touch, etc.
    nAlarming = 0; // stop alarm
  if(!wifi.isCfg())
    if(utime.check(ee.tz))
      checkSched(true);  // initialize

  if(sht.service())
  {
    float newtemp, newrh;
    if(bCF)
      newtemp = sht.getTemperatureC() * 10;
    else
      newtemp = sht.getTemperatureF() * 10;
    newtemp += ee.tAdj[1]; // calibrated temp value
    
    tempMedian[0].add(newtemp);
    tempMedian[0].getAverage(2, newtemp);
    tempMedian[1].add(sht.getRh() * 10);
    tempMedian[1].getAverage(2, newrh);
    
    if(display.m_roomTemp != newtemp)
    {
      display.m_roomTemp = newtemp;
      display.m_rh = newrh;
      sendState();
    }
  }

#ifdef VIBE
  if(vibeCnt)
  {
    delay(vibePeriod);
    pinMode(VIBE, INPUT);
    digitalWrite(VIBE, HIGH);
    if(--vibeCnt)
    {
      pinMode(VIBE, OUTPUT);
      digitalWrite(VIBE, LOW);
    }
  }
#endif

#ifdef MOTION
  if(digitalRead(MOTION) != bMotion)
  {
    bMotion = digitalRead(MOTION);
    if(bMotion)
    {
      sendState();
      display.screen(true);
      LightSwitch(0, 1);
    }
  }
#endif

  if(toneEnd) // long tone delay
  {
    if(millis() >= toneEnd)
    {
      toneEnd = 0;
      analogWrite(TONE, 0);
    }
  }

  switch(display.m_LightSet)
  {
    case 1: // light 1 on/off 
      LightSwitch(0, display.m_bLightOn);
      display.m_LightSet = 0;
      break;
    case 2:
      LightSwitch(1, display.m_nLightLevel);
      display.m_LightSet = 0;
      break;
  }

  if(sec_save != second()) // only do stuff once per second
  {
    sec_save = second();

    checkTemp();

//    digitalWrite(ESP_LED, LOW);
//    delay(5);
//    digitalWrite(ESP_LED, HIGH);
    
    if(--ssCnt == 0)
       sendState();

    display.oneSec();

    if(min_save != minute()) // only do stuff once per minute
    {
      min_save = minute();
      checkSched(false);     // check every minute for next schedule
      if(display.checkAlarms()) // returns true of an alarm == this time
      {
        nAlarming = 60;
        ws.textAll("alert; Alarm");
      }
      
      if( min_save == 0)     // update clock daily (at 2AM for DST)
      {
        if( hour() == 2)
        {
          utime.start();
          updateAll(true);      // update EEPROM daily
        }
        if( mon_save != month() )
        {
          mon_save = month();
          ee.tSecsMon[mon_save-1] = 0;
        }
        ta.add();
        if( updateAll(false) )      // update EEPROM if changed
          ws.textAll("print;EEPROM updated");
        CallHost(Reason_Setup);
      }
      if(min_save == 30)
        ta.add(); // half hour log
    }

    if(nSnoozeTimer)
    {
      if(--nSnoozeTimer == 0)
        nAlarming = 60;
    }

    if(nAlarming) // make noise
    {
      if(--nAlarming)
        Tone(3000, 500);
    }

    if(nWrongPass)    nWrongPass--;
    if(digitalRead(HEAT))
      onCounter++;
    else if(onCounter)
    {
      ee.tSecsMon[month()-1] += onCounter;
      onCounter = 0;
    }
    static uint16_t s = 1;
    if(ee.bEco && display.m_bHeater) // eco mode
    {
      bool bBoost = (display.m_currentTemp < display.m_loTemp - ee.pids[2]); // 0.5 deg diff
      if(bBoost == false)
      {
        if(--s == 0)
        {
          bool bOn = digitalRead(HEAT);
          s = (bOn) ? ee.pids[0] : ee.pids[1]; // off 60 secs /on 180 secs (75%)
          digitalWrite(HEAT, !bOn);
        }
      }
    }

    if(display.m_bHeater != bLastOn || onCounter > (60*60*12)) // total up when it turns off or before 32 bit carry error
    {
      if(bLastOn)
      {
        updateAll( false );
      }
      bLastOn = display.m_bHeater;
      ee.tSecsMon[month()-1] += onCounter;
      onCounter = 0;
    }
  }

  delay(10);
}

void setHeat()
{
  digitalWrite(HEAT, display.m_bHeater);
}

// Check temp to turn heater on and off
void checkTemp()
{
  static RunningMedian<uint16_t,32> tempMedian;
  static uint8_t state = 0;

  switch(state)
  {
    case 0: // start a conversion
      ds.reset();
      ds.select(ds_addr);
      ds.write(0x44, 0);   // start conversion, no parasite power on at the end
      state++;
      return;
    case 1:
      state++;
      break;
    case 2:
      state = 0; // 1 second rest
      return;
  }

  uint8_t data[10];
  uint8_t present = ds.reset();
  ds.select(ds_addr);
  ds.write(0xBE);         // Read Scratchpad

  if(!present)      // safety
  {
    display.m_bHeater = false;
    setHeat();
    ws.textAll("alert; DS18 not present");
    Tone(1000, 100);
    return;
  }

  for ( int i = 0; i < 9; i++)          // we need 9 bytes
    data[i] = ds.read();

  if(OneWire::crc8( data, 8) != data[8])  // bad CRC
  {
    display.m_bHeater = false;
    setHeat();
    ws.textAll("alert;Invalid CRC");
    Tone(1000, 100);
    return;
  }

  uint16_t raw = (data[1] << 8) | data[0];

  if(raw > 630 || raw < 200){ // first reading is always 1360 (0x550)
    ws.textAll("alert;DS error");
    Tone(1000, 100);
    return;
  }

  if(bCF)
    tempMedian.add(( raw * 625) / 1000 );  // to 10x celcius
  else
    tempMedian.add( (raw * 1125) / 1000 + 320) ; // 10x fahrenheit

  float t;
  tempMedian.getAverage(2, t);
  uint16_t newTemp = t;

  static uint16_t oldHT;
  if(newTemp != display.m_currentTemp || display.m_hiTemp != oldHT)
  {
    display.m_currentTemp = newTemp;
    oldHT = display.m_hiTemp;
    sendState();
  }

  if(display.m_currentTemp <= display.m_loTemp && display.m_bHeater == false)
  {
    display.m_bHeater = true;
    setHeat();
    sendState();
    ta.add();
  }
  else if(display.m_currentTemp >= display.m_hiTemp && display.m_bHeater == true)
  {
    display.m_bHeater = false;
    setHeat();
    sendState();
    ta.add();
  }
}

void sendState()
{
  ws.textAll( dataJson() );
  ssCnt = ee.rate;
}

// Check the buttons
void checkButtons()
{
  static bool bState;
  static bool bNewState;
  static bool lbState;
  static long debounce;
  static long lRepeatMillis;
  static bool bRepeat;

#define REPEAT_DELAY 200 // increase for slower repeat

    bNewState = digitalRead(BTN);
    if(bNewState != lbState)
      debounce = millis(); // reset on state change

    bool bInvoke = false;
    if((millis() - debounce) > 30)
    {
      if(bNewState != bState) // press or release
      {
        bState = bNewState;
        if (bState == LOW) // pressed
        {
          if(display.isOff()) // skip first press with display off
          {
            display.screen(true);
          }else
            bInvoke = true;
          lRepeatMillis = millis(); // initial increment (doubled)
          bRepeat = false;
        }
      }
      else if(bState == LOW) // holding down
      {
        if( (millis() - lRepeatMillis) > REPEAT_DELAY * (bRepeat?1:2) )
        {
          bInvoke = true;
          lRepeatMillis = millis();
          bRepeat = true;
        }
      }
    }

    if(bInvoke)
    {
       nAlarming = 0;
       nSnoozeTimer = 60*5; // snooze for 5m
    }
    lbState = bNewState;
}

void changeTemp(int delta, bool bAll)
{
  if(ee.bVaca) return;
  if(bAll)
  {
    for(int i = 0; i < ee.schedCnt; i++)
      ee.schedule[i].setTemp += delta;
  }
  else if(ee.bAvg) // bump both used in avg mode
  {
    ee.schedule[display.m_schInd].setTemp += delta;
    ee.schedule[ (display.m_schInd+1) % ee.schedCnt].setTemp += delta;
  }
  else
  {
    ee.schedule[display.m_schInd].setTemp += delta;    
  }
  checkLimits();
  checkSched(false);     // update temp
}

void checkLimits()
{
  for(int i = 0; i < ee.schedCnt; i++)
  {
    if(bCF)
      ee.schedule[i].setTemp = constrain(ee.schedule[i].setTemp, 155, 322); // sanity check (15.5~32.2)
    else
      ee.schedule[i].setTemp = constrain(ee.schedule[i].setTemp, 600, 900); // sanity check (60~90)
    ee.schedule[i].thresh = constrain(ee.schedule[i].thresh, 1, 100); // (50~80)
  }
}

void checkSched(bool bUpdate)
{
  long timeNow = (hour()*60) + minute();

  if(bUpdate)
  {
    display.m_schInd = ee.schedCnt - 1;
    for(int i = 0; i < ee.schedCnt; i++) // any time check
      if(timeNow >= ee.schedule[i].timeSch && timeNow < ee.schedule[i+1].timeSch)
      {
        display.m_schInd = i;
        break;
      }
  }
  else for(int i = 0; i < ee.schedCnt; i++) // on-time check
  {
    if(timeNow == ee.schedule[i].timeSch)
    {
      display.m_schInd = i;
      break;
    }
  }

  display.m_hiTemp = ee.bVaca ? ee.vacaTemp : ee.schedule[display.m_schInd].setTemp;
  display.m_loTemp = ee.bVaca ? (display.m_hiTemp - 10) : (display.m_hiTemp - ee.schedule[display.m_schInd].thresh);
  int thresh = ee.schedule[display.m_schInd].thresh;

  if(!ee.bVaca && ee.bAvg) // averageing mode
  {
    int start = ee.schedule[display.m_schInd].timeSch;
    int range;
    int s2;

    // Find minute range between schedules
    if(display.m_schInd == ee.schedCnt - 1) // rollover
    {
      s2 = 0;
      range = ee.schedule[s2].timeSch + (24*60) - start;
    }
    else
    {
      s2 = display.m_schInd + 1;
      range = ee.schedule[s2].timeSch - start;
    }

    int m = (hour() * 60) + minute(); // current TOD in minutes

    if(m < start) // offset by start of current schedule
      m -= start - (24*60); // rollover
    else
      m -= start;

    display.m_hiTemp = tween(ee.schedule[display.m_schInd].setTemp, ee.schedule[s2].setTemp, m, range);
    thresh = tween(ee.schedule[display.m_schInd].thresh, ee.schedule[s2].thresh, m, range);
    display.m_loTemp = display.m_hiTemp - thresh;
  }
}

// avarge value at current minute between times
int tween(int t1, int t2, int m, int range)
{
  if(range == 0) range = 1; // div by zero check
  int t = (t2 - t1) * (m * 100 / range) / 100;
  return t + t1;
}

void Tone(unsigned int frequency, uint32_t duration)
{
  analogWriteFreq(frequency);
  analogWrite(TONE, 500);
  if(duration <= 20)
  {
    delay(duration);
    analogWrite(TONE, 0);
  }
  else // >20ms is handled in loop
  {
    toneEnd = millis() + duration;
  }
}
