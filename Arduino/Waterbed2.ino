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

//#define SDEBUG
//uncomment to enable Arduino IDE Over The Air update code
#define OTA_ENABLE
//#define USE_SPIFFS
#define ENABLE_HVAC_SENSOR

#include <Wire.h>
#include <EEPROM.h>
#include <ESP8266mDNS.h>
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
#include "music.h"

#include <WebSocketsClient.h> // https://github.com/Links2004/arduinoWebSockets
//switch WEBSOCKETS_NETWORK_TYPE to NETWORK_ESP8266_ASYNC in WebSockets.h
#if (WEBSOCKETS_NETWORK_TYPE != NETWORK_ESP8266_ASYNC)
#error "network type must be ESP8266 ASYNC!"
#endif

const char controlPassword[] = "password";    // device password for modifying any settings
const int serverPort = 80;                    // HTTP port
const char hostName[] = "Waterbed";
#define WBID 0x4254574d // Sensor ID for thermostat MWTB   0x42545747 = GWTB

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
Music mus;

OneWire ds(DS18B20);
byte ds_addr[8];
IPAddress lastIP;
int nWrongPass;

SHT21 sht(SDA, SCL, 4);
uint16_t light;

Display display;
eeMem ee;

AsyncWebServer server( serverPort );
AsyncWebSocket ws("/ws"); // access at ws://[esp ip]/ws

UdpTime utime;

bool bCF = false;

uint32_t onCounter;
bool bMotion = true;
bool bNotifAck = false;
uint8_t nAlarming;
uint16_t nSnoozeTimer;

uint32_t nHeatCnt;
uint32_t nCoolCnt;
uint32_t nOvershootCnt;
uint32_t nOvershootPeak;
uint32_t nHeatETA;
uint32_t nCoolETA;
bool bBoost;
uint16_t nOvershootStartTemp;
uint16_t nOvershootEndTemp;
int16_t nOvershootTempDiff;
uint32_t nOvershootTime;
bool bTxTemp;

bool bConfigDone = false;
bool bStarted = false;
uint32_t connectTimer;

void jsonCallback(int16_t iEvent, uint16_t iName, int iValue, char *psValue);
JsonParse jsonParse(jsonCallback);
void jsonPushCallback(int16_t iEvent, uint16_t iName, int iValue, char *psValue);
JsonClient jsonPush(jsonPushCallback);

bool updateAll(bool bForce)
{
  return ee.update(bForce);
}

String dataJson()
{
  jsonString js("state");

  js.Var("t", (uint32_t)(now() - ((ee.tz + utime.getDST()) * 3600)) );
  js.Var("waterTemp", String((float)display.m_currentTemp / 10, 1) );
  js.Var("setTemp", String((float)ee.schedule[display.m_season][display.m_schInd].setTemp / 10, 1) );
  js.Var("hiTemp",  String((float)display.m_hiTemp / 10, 1) );
  js.Var("loTemp",  String((float)display.m_loTemp / 10, 1) );
  js.Var("on",   digitalRead(HEAT));
  js.Var("temp", String((float)display.m_roomTemp / 10, 1) );
  js.Var("rh",   String((float)display.m_rh / 10, 1) );
  js.Var("c",    String((bCF) ? "C" : "F"));
  js.Var("oc",   onCounter );
  js.Var("mot",  bMotion);
  js.Var("eta",  nHeatETA);
  js.Var("cooleta",  nCoolETA);
  js.Var("notif",  bNotifAck);
  bNotifAck = false;
  return js.Close();
}

String setJson() // settings
{
  jsonString js("set");

  js.Var("vt", String((float)ee.vacaTemp / 10, 1) );
  js.Var("o",   0);
  js.Var("tz",  ee.tz);
  js.Var("avg", ee.bAvg);
  js.Var("ppkwh", ee.ppkwh);
  js.Var("vo",  ee.bVaca);
  js.Var("idx", display.m_schInd);
  js.Array("cnt", ee.schedCnt, 2);
  js.Var("w",  ee.watts);
  js.Var("r",  ee.rate);
  js.Var("e",  ee.bEco);
  js.Var("season", display.m_season);
  IPAddress hip(ee.hostIP);
  IPAddress lip(ee.lightIP[0]);
  js.Var("hip", hip.toString());
  js.Var("lip", lip.toString());
  js.Array("item", ee.schedule, 2);
  js.Array("ts", ee.tSecsMon, 12);
  js.Array("ppkwm", ee.ppkwm, 12);
  return js.Close();
}

void ePrint(String s)
{
  ws.textAll(s);
}

const char *jsonListCmd[] = {
  "cmd",
  "key",
  "oled",
  "TZ",
  "avg",
  "cnt",
  "tadj",
  "ppkwh",
  "vaca",
  "vacatemp",
  "I",
  "N", // 10
  "S",
  "T",
  "H",
  "vibe",
  "watts",
  "dot",
  "save",
  "aadj",
  "eco",
  "outtemp", // 20
  "outrh",
  "notif",
  "notifCancel",
  "rate",
  "hostip",
  "lightip",
  "music",
  "send",
  "restart",
  NULL
};

bool bKeyGood;

void parseParams(AsyncWebServerRequest *request)
{
  int16_t val;

  if (nWrongPass && request->client()->remoteIP() != lastIP) // if different IP drop it down
    nWrongPass = 10;

  lastIP = request->client()->remoteIP();

  for ( uint8_t i = 0; i < request->params(); i++ )
  {
    AsyncWebParameter* p = request->getParam(i);
    String s = request->urlDecode(p->value());

    uint8_t idx;
    for (idx = 1; jsonListCmd[idx]; idx++)
      if ( p->name().equals(jsonListCmd[idx]) )
        break;
    if (jsonListCmd[idx] == NULL)
      return;
    int iValue = s.toInt();
    if (s == "true") iValue = 1;

    jsonCallback(0, idx - 1, iValue, (char *)s.c_str());
  }
}

void jsonCallback(int16_t iEvent, uint16_t iName, int iValue, char *psValue)
{
  if (!bKeyGood && iName)
  {
    if (nWrongPass == 0)
      nWrongPass = 10;
    else if ((nWrongPass & 0xFFFFF000) == 0 ) // time doubles for every high speed wrong password attempt.  Max 1 hour
      nWrongPass <<= 1;
    return; // only allow for key
  }
  char *p, *p2;
  static int beepPer = 1000;
  static int item = 0;

  switch (iEvent)
  {
    case 0: // cmd
      switch (iName)
      {
        case 0: // key
          if (!strcmp(psValue, controlPassword)) // first item must be key
            bKeyGood = true;
          break;
        case 1: // OLED
          display.screen(true);
          break;
        case 2: // TZ
          ee.tz = iValue;
          utime.start();
          break;
        case 3: // avg
          ee.bAvg = iValue;
          break;
        case 4: // cnt
          ee.schedCnt[display.m_season] = constrain(iValue, 1, 8);
          ws.textAll(setJson()); // update all the entries
          break;
        case 5: // tadj
          changeTemp(iValue, false);
          ws.textAll(setJson()); // update all the entries
          break;
        case 6: // ppkw
          ee.ppkwm[month() - 1] = ee.ppkwh = iValue;
          break;
        case 7: // vaca
          ee.bVaca = iValue;
          break;
        case 8: // vacatemp
          if (bCF)
            ee.vacaTemp = constrain( (int)(atof(psValue) * 10), 100, 290); // 10-29C
          else
            ee.vacaTemp = constrain( (int)(atof(psValue) * 10), 600, 840); // 60-84F
          break;
        case 9: // I
          item = iValue;
          break;
        case 10: // N
          break;
        case 11: // S
          p = strtok(psValue, ":");
          p2 = strtok(NULL, "");
          if (p && p2) {
            iValue *= 60;
            iValue += atoi(p2);
          }
          ee.schedule[display.m_season][item].timeSch = iValue;
          break;
        case 12: // T
          ee.schedule[display.m_season][item].setTemp = atof(psValue) * 10;
          break;
        case 13: // H
          ee.schedule[display.m_season][item].thresh = (int)(atof(psValue) * 10);
          checkLimits();      // constrain and check new values
          checkSched(true);   // reconfigure to new schedule
          break;
        case 14: // vibe (vibe period)
#ifdef VIBE
          vibePeriod = iValue;
          vibeCnt = 1;
          pinMode(VIBE, OUTPUT);
          digitalWrite(VIBE, LOW);
#endif
          break;
        case 15: // watts
          ee.watts = iValue;
          break;
        case 16: // dot displayOnTimer
          if (iValue)
            display.screen(true);
          break;
        case 17: // save
          updateAll(true);
          break;
        case 18: // aadj
          changeTemp(iValue, true);
          ws.textAll(setJson()); // update all the entries
          break;
        case 19: // eco
          ee.bEco = iValue ? true : false;
          setHeat();
          break;
        case 20: // outtemp
          display.m_outTemp = iValue;
          break;
        case 21: // outrh
          display.m_outRh = iValue;
          break;
        case 22: // notif
          display.Notification(psValue, lastIP);
          break;
        case 23: // notifCancel
          display.NotificationCancel(psValue);
          break;
        case 24: // rate
          if (iValue == 0)
            break; // don't allow 0
          ee.rate = iValue;
          sendState();
          break;
        case 25: // host IP / port  (call from host with ?h=80)
          ee.hostIP[0] = lastIP[0];
          ee.hostIP[1] = lastIP[1];
          ee.hostIP[2] = lastIP[2];
          ee.hostIP[3] = lastIP[3];
          ee.hostPort = iValue ? iValue : 80;
          break;
        case 26: // lightIP
          {
            IPAddress ip;
            ip.fromString(psValue);
            ee.lightIP[0][0] = ip[0];
            ee.lightIP[0][1] = ip[1];
            ee.lightIP[0][2] = ip[2];
            ee.lightIP[0][3] = ip[3];
          }
          break;
        case 27: // music
          mus.play(iValue);
          break;
        case 28: // send
          bTxTemp = iValue ? true : false;
          break;
        case 29: // restart
          ESP.reset();
          break;
      }
      break;
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

const char *jsonListPush[] = {
  "time",
  "time", // 0
  "ppkw",
  "on",
  "level",
  NULL
};

void jsonPushCallback(int16_t iEvent, uint16_t iName, int iValue, char *psValue)
{
  switch (iEvent)
  {
    case -1: // status
      break;
    case 0: // time
      switch (iName)
      {
        case 0: // time
          if (iValue < now() + ((ee.tz + utime.getDST()) * 3600) )
            break;
          setTime(iValue + ( (ee.tz + utime.getDST() ) * 3600));
          break;
        case 1: // ppkw
          ee.ppkwm[month() - 1] = ee.ppkwh = iValue;
          break;
        case 2: // on
          display.m_bLightOn = iValue ? true : false;
          break;
        case 3: // level
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
uint8_t qI;

void checkQueue()
{
  if (WiFi.status() != WL_CONNECTED)
    return;

  static uint8_t qIdx;

  if (qIdx == qI || queue[qIdx].port == 0) // Nothing to do
    return;

  if ( jsonPush.begin(queue[qIdx].ip, queue[qIdx].sUri.c_str(), queue[qIdx].port, false, false, NULL, NULL, 1) )
  {
    jsonPush.addList(jsonListPush);
    queue[qIdx].port = 0;
    if (++qIdx >= CQ_CNT)
      qIdx = 0;
  }
}

void callQueue(IPAddress ip, String sUri, uint16_t port)
{
  if (queue[qI].port == 0)
  {
    queue[qI].ip = ip;
    queue[qI].sUri = sUri;
    queue[qI].port = port;
  }

  if (++qI >= CQ_CNT)
    qI = 0;
}

void CallHost(reportReason r)
{
  if (ee.hostIP[0] == 0) // no host set
    return;

  String sUri = String("/wifi?name=\"");
  sUri += hostName;
  sUri += "\"&reason=";

  switch (r)
  {
    case Reason_Setup:
      sUri += "setup&port="; sUri += serverPort;
      sUri += "&on=1";
      break;
    case Reason_Motion:
      sUri += "motion";
      break;
    case Reason_Notif:
      sUri += "notif";
      break;
  }

  IPAddress ip(ee.hostIP);
  callQueue(ip, sUri, ee.hostPort);
}

void LightSwitch(uint8_t t, uint8_t v)
{
  if (ee.lightIP[0][0] == 0)
    return;
  String sUri = "/wifi?key=";
  sUri += controlPassword;
  sUri += "&";
  switch (t)
  {
    case 0: // switch
      sUri += "pwr=";
      break;
    case 1: // level
      sUri += "level=";
      break;
  }
  sUri += v;
  IPAddress ip(ee.lightIP[0]);
  callQueue(ip, sUri, 80);
}

WebSocketsClient wsc;
bool bWscConnected;

void connectDimmer()
{
  wsc.onEvent(webSocketEvent);
  IPAddress ip(ee.lightIP[0]);
  wsc.begin(ip, 80, "/ws");
}

void webSocketEvent(WStype_t type, uint8_t * payload, size_t length)
{
  switch (type)
  {
    case WStype_DISCONNECTED:
      bWscConnected = false;
      break;
    case WStype_CONNECTED:
      bWscConnected = true;
      break;
    case WStype_TEXT:
      {
        char *pCmd = strtok((char *)payload, ";");
        char *pData = strtok(NULL, "");
        if (pCmd == NULL || pData == NULL) break;
        //        remoteParse.process(pCmd, pData);
      }
      break;
  }
}


void onWsEvent(AsyncWebSocket * server, AsyncWebSocketClient * client, AwsEventType type, void * arg, uint8_t *data, size_t len)
{ //Handle WebSocket event
  static bool bRestarted = true;

  switch (type)
  {
    case WS_EVT_CONNECT:      //client connected
      if (bRestarted)
      {
        bRestarted = false;
        client->text("alert;Restarted");
      }

      client->text(dataJson());
      client->text(setJson());
      client->text( ta.get() );
      break;
    case WS_EVT_DISCONNECT:    //client disconnected
      break;
    case WS_EVT_ERROR:    //error was received from the other end
      break;
    case WS_EVT_PONG:    //pong message was received (in response to a ping request maybe)
      break;
    case WS_EVT_DATA:  //data packet
      AwsFrameInfo * info = (AwsFrameInfo*)arg;
      if (info->final && info->index == 0 && info->len == len) {
        //the whole message is in a single frame and we got all of it's data
        if (info->opcode == WS_TEXT) {
          data[len] = 0;

          char *pCmd = strtok((char *)data, ";"); // assume format is "name;{json:x}"
          char *pData = strtok(NULL, "");
          if (pCmd == NULL || pData == NULL) break;
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
  delay(1000);
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
  WiFi.mode(WIFI_STA);

  if ( ee.szSSID[0] )
  {
    WiFi.begin(ee.szSSID, ee.szSSIDPassword);
    WiFi.setHostname(hostName);
    bConfigDone = true;
  }
  else
  {
    Serial.println("No SSID. Waiting for EspTouch.");
    WiFi.beginSmartConfig();
  }
  connectTimer = now();

#ifdef USE_SPIFFS
  SPIFFS.begin();
  server.addHandler(new SPIFFSEditor("admin", controlPassword));
#endif

  // attach AsyncWebSocket
  ws.onEvent(onWsEvent);
  server.addHandler(&ws);

#ifdef ENABLE_HVAC_SENSOR
  // Find HVAC
  int cnt = MDNS.queryService("iot", "tcp");
  for (int i = 0; i < cnt; ++i)
  {
    char szName[38];
    MDNS.hostname(i).toCharArray(szName, sizeof(szName));
    strtok(szName, "."); // remove .local

    if (!strcmp(szName, "HVAC"))
    {
      ee.hvacIP[0] = MDNS.IP(i)[0]; // update IP
      ee.hvacIP[1] = MDNS.IP(i)[1];
      ee.hvacIP[2] = MDNS.IP(i)[2];
      ee.hvacIP[3] = MDNS.IP(i)[3];
      break;
    }
  }
#endif

  server.on ( "/", HTTP_GET, [](AsyncWebServerRequest * request)
  {
    // Send nothing
  });
  server.on ( "/iot", HTTP_GET | HTTP_POST, [](AsyncWebServerRequest * request )
  {
    parseParams(request);
#ifdef USE_SPIFFS
    request->send(SPIFFS, "/index.htm");
#else
    request->send_P(200, "text/html", index_page);
#endif
  });
  server.on ( "/s", HTTP_GET | HTTP_POST, handleS );
  server.on ( "/set", HTTP_GET, [](AsyncWebServerRequest * request) {
    request->send( 200, "text/json", setJson() );
  });
  server.on ( "/json", HTTP_GET, [](AsyncWebServerRequest * request) {
    request->send( 200, "text/json", dataJson() );
  });

  server.onNotFound([](AsyncWebServerRequest * request) { // be silent
    //    request->send(404);
  });
  server.on("/heap", HTTP_GET, [](AsyncWebServerRequest * request) {
    request->send(200, "text/plain", String(ESP.getFreeHeap()));
  });
  server.on("/scan", HTTP_GET, [](AsyncWebServerRequest * request) {
    String json = "[";
    int n = WiFi.scanComplete();
    if (n == -2) {
      WiFi.scanNetworks(true);
    } else if (n) {
      for (int i = 0; i < n; ++i) {
        if (i) json += ",";
        jsonString js;

        js.Var("rssi", String(WiFi.RSSI(i)) );
        js.Var("ssid", WiFi.SSID(i) );
        js.Var("bssid", WiFi.BSSIDstr(i) );
        js.Var("channel", String(WiFi.channel(i)) );
        js.Var("secure", String(WiFi.encryptionType(i)) );
        js.Var("hidden", String(WiFi.isHidden(i) ? "true" : "false") );
        json += js.Close();
      }
      WiFi.scanDelete();
      if (WiFi.scanComplete() == -2) {
        WiFi.scanNetworks(true);
      }
    }
    json += "]";
    request->send(200, "text/json", json);
  });

  server.on("/favicon.ico", HTTP_GET, [](AsyncWebServerRequest * request) {
#ifdef USE_SPIFFS
    request->send(SPIFFS, "/favicon.ico");
#else
    AsyncWebServerResponse *response = request->beginResponse_P(200, "image/x-icon", favicon, sizeof(favicon));
    response->addHeader("Content-Encoding", "gzip");
    request->send(response);
#endif
  });

  server.onFileUpload([](AsyncWebServerRequest * request, String filename, size_t index, uint8_t *data, size_t len, bool final) {
  });
  server.onRequestBody([](AsyncWebServerRequest * request, uint8_t *data, size_t len, size_t index, size_t total) {
  });
  server.begin();

#ifdef OTA_ENABLE
  ArduinoOTA.setHostname(hostName);
  ArduinoOTA.begin();
  ArduinoOTA.onStart([]() {
    digitalWrite(HEAT, LOW);
    ee.tSecsMon[month() - 1] += onCounter;
    updateAll( true );
  });
#endif
  jsonParse.addList(jsonListCmd);

  if ( ds.search(ds_addr) )
  {
#ifdef SDEBUG
    Serial.print("OneWire device: "); // 28 22 92 29 7 0 0 6B
    for ( int i = 0; i < 8; i++) {
      Serial.print(ds_addr[i], HEX);
      Serial.print(" ");
    }
    Serial.println("");
    if ( OneWire::crc8( ds_addr, 7) != ds_addr[7]) {
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

  sht.init();
  digitalWrite(ESP_LED, HIGH);
  mus.add(2000, 50);
  mus.add(5000, 100);
}

uint8_t ssCnt = 30;
uint8_t motCnt;

void loop()
{
  static RunningMedian<uint16_t, 24> tempMedian[2];
  static int8_t min_save, sec_save, mon_save = -1;
  static bool bLastOn;

  MDNS.update();
#ifdef OTA_ENABLE
  ArduinoOTA.handle();
#endif
  checkButtons();
  checkQueue();
  if (display.checkNextion()) // check for touch, etc.
    nAlarming = 0; // stop alarm

  if (WiFi.status() == WL_CONNECTED)
    if (utime.check(ee.tz))
      checkSched(true);  // initialize

  if (sht.service())
  {
    float newtemp, newrh;
    if (bCF)
      newtemp = sht.getTemperatureC() * 10;
    else
      newtemp = sht.getTemperatureF() * 10;
    newtemp += ee.tAdj[1]; // calibrated temp value

    tempMedian[0].add(newtemp);
    tempMedian[0].getAverage(2, newtemp);
    tempMedian[1].add(sht.getRh() * 10);
    tempMedian[1].getAverage(2, newrh);

    if (display.m_roomTemp != newtemp)
    {
      display.m_roomTemp = newtemp;
      display.m_rh = newrh;
      sendState();
    }
#ifdef ENABLE_HVAC_SENSOR
    static int16_t oldTemp;
    static uint32_t secs;

    if ( (newtemp != oldTemp || now() - secs > 30 ) && WiFi.status() == WL_CONNECTED)
    {
      oldTemp = newtemp;
      secs = now();

      if (bTxTemp)
        updateHvac();
    }
#endif
  }

#ifdef VIBE
  if (vibeCnt)
  {
    delay(vibePeriod);
    pinMode(VIBE, INPUT);
    digitalWrite(VIBE, HIGH);
    if (--vibeCnt)
    {
      pinMode(VIBE, OUTPUT);
      digitalWrite(VIBE, LOW);
    }
  }
#endif

#ifdef MOTION
  if (digitalRead(MOTION) != bMotion)
  {
    bMotion = digitalRead(MOTION);
    if (bMotion)
    {
      sendState();
      display.screen(true);
      LightSwitch(0, 1);
    }
  }
#endif
  mus.service();

  switch (display.m_LightSet)
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

  if (sec_save != second()) // only do stuff once per second
  {
    sec_save = second();

    if(!bConfigDone)
    {
      if( WiFi.smartConfigDone())
      {
        Serial.println("SmartConfig set");
        bConfigDone = true;
        connectTimer = now();
      }
    }
    if(bConfigDone)
    {
      if(WiFi.status() == WL_CONNECTED)
      {
        if(!bStarted)
        {
          Serial.println("WiFi Connected");
          MDNS.begin( hostName );
          bStarted = true;
          utime.start();
          MDNS.addService("iot", "tcp", serverPort);
          WiFi.SSID().toCharArray(ee.szSSID, sizeof(ee.szSSID)); // Get the SSID from SmartConfig or last used
          WiFi.psk().toCharArray(ee.szSSIDPassword, sizeof(ee.szSSIDPassword) );
          updateAll(false);
          display.init();
          connectDimmer();
        }
        if(utime.check(ee.tz))
          checkSched(true);  // initialize
      }
      else if(now() - connectTimer > 10) // failed to connect for some reason
      {
        Serial.println("Connect failed. Starting SmartConfig");
        connectTimer = now();
        ee.szSSID[0] = 0;
        WiFi.mode(WIFI_AP_STA);
        WiFi.beginSmartConfig();
        bConfigDone = false;
        bStarted = false;
      }
    }

    checkTemp();

    //    digitalWrite(ESP_LED, LOW);
    //    delay(5);
    //    digitalWrite(ESP_LED, HIGH);

    if (--ssCnt == 0)
      sendState();

    display.oneSec();

    if (min_save != minute()) // only do stuff once per minute
    {
      min_save = minute();
      checkSched(false);     // check every minute for next schedule
      if (display.checkAlarms()) // returns true of an alarm == this time
      {
        nAlarming = 60;
        ws.textAll("alert; Alarm");
      }

      if ( min_save == 0)
      {
        ta.add();
        if ( hour() == 2)     // update clock daily (at 2AM for DST)
        {
          utime.start();
          updateAll(false);    // update EEPROM daily
        }
        else if ( (hour() & 1) == 0) // even hour
        {
          updateAll(false);   // update EEPROM if changed
        }
        if ( mon_save != month() )
        {
          if (mon_save >= 0) // restart check
            ee.tSecsMon[month() - 1] = 0;
          mon_save = month();
        }
        CallHost(Reason_Setup);
      }
      if (min_save == 30)
        ta.add(); // half hour log

      display.m_season = utime.getDST(); // simple for now
    }

    if (nSnoozeTimer)
    {
      if (--nSnoozeTimer == 0)
        nAlarming = 60;
    }

    if (nAlarming) // make noise
    {
      if (--nAlarming)
        mus.add(3000, 500);
    }

    if (nWrongPass)
      nWrongPass--;
    if (digitalRead(HEAT))
      onCounter++;
    else if (onCounter)
    {
      ee.tSecsMon[month() - 1] += onCounter;
      onCounter = 0;
    }
    if (display.m_bHeater == false && nOvershootCnt)
      nOvershootCnt++;
    static uint16_t s = 1;
    if (ee.bEco && display.m_bHeater) // eco mode
    {
      bBoost = (display.m_currentTemp < display.m_loTemp - ee.pids[2]); // 0.5 deg diff
      if (bBoost == false)
      {
        if (--s == 0)
        {
          bool bOn = digitalRead(HEAT);
          s = (bOn) ? ee.pids[0] : ee.pids[1]; // off 60 secs /on 180 secs (75%)
          digitalWrite(HEAT, !bOn);
        }
      }
    }

    if (display.m_bHeater != bLastOn || onCounter > (60 * 60 * 12)) // total up when it turns off or before 32 bit carry error
    {
      if (bLastOn)
      {
        updateAll( false );
      }
      bLastOn = display.m_bHeater;
      ee.tSecsMon[month() - 1] += onCounter;
      onCounter = 0;
    }
    if (display.m_bHeater)
      nHeatCnt++;
    else
      nCoolCnt++;
    if (nHeatETA)
      nHeatETA--;
    if (nCoolETA)
      nCoolETA--;
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
  static RunningMedian<uint16_t, 32> tempMedian;
  static RunningMedian<uint32_t, 8> heatTimeMedian;
  static RunningMedian<uint32_t, 8> coolTimeMedian;
  static uint8_t state = 0;

  switch (state)
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

  if (!present)     // safety
  {
    display.m_bHeater = false;
    setHeat();
    ws.textAll("alert; DS18 not present");
    display.Notification("WARNING\r\nDS18 not detected", (IPAddress)0);
    return;
  }

  for ( int i = 0; i < 9; i++)          // we need 9 bytes
    data[i] = ds.read();

  if (OneWire::crc8( data, 8) != data[8]) // bad CRC
  {
    display.m_bHeater = false;
    setHeat();
    ws.textAll("alert;Invalid CRC");
    display.Notification("WARNING\r\nDS18 CRC error", (IPAddress)0);
    return;
  }

  uint16_t raw = (data[1] << 8) | data[0];

  if (raw > 630 || raw < 200) { // first reading is always 1360 (0x550)
    ws.textAll("alert;DS error");
    display.Notification("WARNING\r\nDS18 error", (IPAddress)0);
    return;
  }

  if (bCF)
    tempMedian.add(( raw * 625) / 1000 );  // to 10x celcius
  else
    tempMedian.add( (raw * 1125) / 1000 + 320) ; // 10x fahrenheit

  float t;
  tempMedian.getAverage(2, t);
  uint16_t newTemp = t;
  static uint16_t oldHT;

  if (display.m_currentTemp == 0 || oldHT == 0) // skip first read
  {
    display.m_currentTemp = newTemp;
    oldHT = display.m_hiTemp;
    return;
  }

  if (newTemp <= display.m_loTemp && display.m_bHeater == false)
  {
    display.m_bHeater = true;
    setHeat();
    ta.add();
  }
  else if (newTemp >= display.m_hiTemp && display.m_bHeater == true)
  {
    display.m_bHeater = false;
    setHeat();
    nHeatETA = 0;
    nOvershootCnt = 1;
    nOvershootStartTemp = newTemp;
    ta.add();
  }

  if (newTemp == display.m_currentTemp && display.m_hiTemp == oldHT)
    return;

  int16_t chg = newTemp - display.m_currentTemp;

  if (display.m_bHeater)
  {
    if (nHeatCnt > 120 && chg > 0)
    {
      if (!ee.bEco || !bBoost) // don't add slower heating
      {
        heatTimeMedian.add(nHeatCnt);
      }
      float fCnt;
      heatTimeMedian.getAverage(fCnt);
      uint32_t ct = fCnt;
      int16_t tDiff = display.m_hiTemp - newTemp;
      nHeatETA = ct * tDiff;
      /*      String s = "print;heatETA ";
            s += nHeatETA;
            s += " ";
            s += ct;
            s += " ";
            s += tDiff;
            ws.textAll(s);
      */
      int16_t ti = hour() * 60 + minute() + (nHeatETA / 60);

      int16_t tt = tempAtTime( ti ); // get real target temp
      tDiff = tt - newTemp;

      if (tDiff < 0) tDiff = 0;
      nHeatETA = ct * tDiff;
      nHeatCnt = 0;
    }
    else
    {
    }
  }
  else if (nCoolCnt > 120)
  {
    if (chg < 0)
    {
      int16_t tDiff = newTemp - display.m_loTemp;
      coolTimeMedian.add(nCoolCnt);
      float fCnt;
      coolTimeMedian.getAverage(fCnt);
      uint32_t ct = fCnt;
      nCoolETA = ct * tDiff;
      nCoolCnt = 0;
      if (nOvershootPeak)
      {
        nOvershootTime = nOvershootPeak;
        nOvershootTempDiff = nOvershootEndTemp - nOvershootStartTemp;

        if (ee.nOvershootTempDiff == 0) // fix for divide by 0
          ee.nOvershootTempDiff = 1;
        if (nOvershootTempDiff == 0) // fix for divide by 0
          nOvershootTempDiff = 1;
        if ((ee.nOvershootTime == 0) || (ee.nOvershootTime && (ee.nOvershootTime / ee.nOvershootTempDiff) < (nOvershootTime / nOvershootTempDiff)) ) // Try to eliminate slosh rise
        {
          ee.nOvershootTempDiff = nOvershootTempDiff;
          ee.nOvershootTime = nOvershootTime;
        }
      }
      nOvershootCnt = 0;
      nOvershootPeak = 0;
    }
    else if (nOvershootCnt) // increasing temp after heater off
    {
      nOvershootPeak = nOvershootCnt;
      nOvershootEndTemp = newTemp;
    }
  }

  display.m_currentTemp = newTemp;
  oldHT = display.m_hiTemp;
  sendState();
}

String timeFmt(uint32_t v)
{
  String s;
  uint32_t m = v / 60;
  uint8_t h = m / 60;
  m %= 60;
  v %= 60;
  s += h;
  s += ":";
  if (m < 10) s += "0";
  s += m;
  s += ":";
  if (v < 10) s += "0";
  s += v;
  return s;
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
  if (bNewState != lbState)
    debounce = millis(); // reset on state change

  bool bInvoke = false;
  if ((millis() - debounce) > 30)
  {
    if (bNewState != bState) // press or release
    {
      bState = bNewState;
      if (bState == LOW) // pressed
      {
        if (display.isOff()) // skip first press with display off
          display.screen(true);
        else
          bInvoke = true;
        lRepeatMillis = millis(); // initial increment (doubled)
        bRepeat = false;
      }
    }
    else if (bState == LOW) // holding down
    {
      if ( (millis() - lRepeatMillis) > REPEAT_DELAY * (bRepeat ? 1 : 2) )
      {
        bInvoke = true;
        lRepeatMillis = millis();
        bRepeat = true;
      }
    }
  }

  if (bInvoke)
  {
    nAlarming = 0;
    nSnoozeTimer = 60 * 5; // snooze for 5m
  }
  lbState = bNewState;
}

void changeTemp(int delta, bool bAll)
{
  if (ee.bVaca) return;
  if (bAll)
  {
    for (int i = 0; i < ee.schedCnt[display.m_season]; i++)
      ee.schedule[display.m_season][i].setTemp += delta;
  }
  else if (ee.bAvg) // bump both used in avg mode
  {
    ee.schedule[display.m_season][display.m_schInd].setTemp += delta;
    ee.schedule[display.m_season][ (display.m_schInd + 1) % ee.schedCnt[display.m_season]].setTemp += delta;
  }
  else
  {
    ee.schedule[display.m_season][display.m_schInd].setTemp += delta;
  }
  checkLimits();
  checkSched(false);     // update temp
}

void checkLimits()
{
  for (int i = 0; i < ee.schedCnt[display.m_season]; i++)
  {
    if (bCF)
      ee.schedule[display.m_season][i].setTemp = constrain(ee.schedule[display.m_season][i].setTemp, 155, 322); // sanity check (15.5~32.2)
    else
      ee.schedule[display.m_season][i].setTemp = constrain(ee.schedule[display.m_season][i].setTemp, 600, 900); // sanity check (60~90)
    ee.schedule[display.m_season][i].thresh = constrain(ee.schedule[display.m_season][i].thresh, 1, 100); // (50~80)
  }
}

void checkSched(bool bUpdate)
{
  long timeNow = (hour() * 60) + minute();

  if (bUpdate)
  {
    display.m_schInd = ee.schedCnt[display.m_season] - 1;
    for (int i = 0; i < ee.schedCnt[display.m_season]; i++) // any time check
      if (timeNow >= ee.schedule[display.m_season][i].timeSch && timeNow < ee.schedule[display.m_season][i + 1].timeSch)
      {
        display.m_schInd = i;
        break;
      }
  }
  else for (int i = 0; i < ee.schedCnt[display.m_season]; i++) // on-time check
    {
      if (timeNow == ee.schedule[display.m_season][i].timeSch)
      {
        display.m_schInd = i;
        break;
      }
    }

  display.m_hiTemp = ee.bVaca ? ee.vacaTemp : ee.schedule[display.m_season][display.m_schInd].setTemp;
  display.m_loTemp = ee.bVaca ? (display.m_hiTemp - 10) : (display.m_hiTemp - ee.schedule[display.m_season][display.m_schInd].thresh);
  int thresh = ee.schedule[display.m_season][display.m_schInd].thresh;

  if (!ee.bVaca && ee.bAvg) // averageing mode
  {
    int start = ee.schedule[display.m_season][display.m_schInd].timeSch;
    int range;
    int s2;

    // Find minute range between schedules
    if (display.m_schInd == ee.schedCnt[display.m_season] - 1) // rollover
    {
      s2 = 0;
      range = ee.schedule[display.m_season][s2].timeSch + (24 * 60) - start;
    }
    else
    {
      s2 = display.m_schInd + 1;
      range = ee.schedule[display.m_season][s2].timeSch - start;
    }

    int m = (hour() * 60) + minute(); // current TOD in minutes

    if (m < start) // offset by start of current schedule
      m -= start - (24 * 60); // rollover
    else
      m -= start;

    display.m_hiTemp = tween(ee.schedule[display.m_season][display.m_schInd].setTemp, ee.schedule[display.m_season][s2].setTemp, m, range);
    thresh = tween(ee.schedule[display.m_season][display.m_schInd].thresh, ee.schedule[display.m_season][s2].thresh, m, range);
    display.m_loTemp = display.m_hiTemp - thresh;
  }
}

uint16_t tempAtTime(uint16_t timeTo) // in minutes
{
  uint8_t idx = ee.schedCnt[display.m_season] - 1;
  uint16_t temp;

  if (ee.bVaca)
  {
    return ee.vacaTemp;
  }

  timeTo %= (24 * 60);

  for (int i = 0; i < ee.schedCnt[display.m_season]; i++) // any time check
    if (timeTo >= ee.schedule[display.m_season][i].timeSch && timeTo < ee.schedule[display.m_season][i + 1].timeSch)
    {
      idx = i;
      break;
    }

  if (!ee.bAvg) // not averageing mode
  {
    return ee.schedule[display.m_season][idx].setTemp;
  }

  int start = ee.schedule[display.m_season][idx].timeSch;
  int range;
  int s2;

  // Find minute range between schedules
  if (idx == ee.schedCnt[display.m_season] - 1) // rollover
  {
    s2 = 0;
    range = ee.schedule[display.m_season][s2].timeSch + (24 * 60) - start;
  }
  else
  {
    s2 = idx + 1;
    range = ee.schedule[display.m_season][s2].timeSch - start;
  }

  if (timeTo < start) // offset by start of current schedule
    timeTo -= start - (24 * 60); // rollover
  else
    timeTo -= start;
  return tween(ee.schedule[display.m_season][idx].setTemp, ee.schedule[display.m_season][s2].setTemp, timeTo, range);
}

// avarge value at current minute between times
int tween(int t1, int t2, int m, int range)
{
  if (range == 0) range = 1; // div by zero check
  int t = (t2 - t1) * (m * 100 / range) / 100;
  return t + t1;
}

#ifdef ENABLE_HVAC_SENSOR
void updateHvac()
{
  if (ee.hvacIP[0] == 0) // not set
    return;

  String sUri = String("/s?key=");
  sUri += controlPassword;
  sUri += "&rmttemp="; sUri += display.m_roomTemp; sUri += (bCF) ? 'C':'F';
  sUri += "&rmtrh="; sUri += display.m_rh;
  sUri += "&rmtname="; sUri += WBID;

  IPAddress ip(ee.hvacIP);
  String url = ip.toString();
  jsonPush.begin(url.c_str(), sUri.c_str(), 80, false, false, NULL, NULL);
  jsonPush.addList(jsonListPush);
}
#endif
