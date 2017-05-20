/*
Copyright (c) 2016 Theo Arends.  All rights reserved.

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions are met:

- Redistributions of source code must retain the above copyright notice,
  this list of conditions and the following disclaimer.
- Redistributions in binary form must reproduce the above copyright notice,
  this list of conditions and the following disclaimer in the documentation
  and/or other materials provided with the distribution.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
POSSIBILITY OF SUCH DAMAGE.

Most of basic code based on Theo Arends and his Sonoff
custom firmware project, https://github.com/arendst/Sonoff-MQTT-OTA-Arduino

*/

#define VERSION                0x01000003   // 1.03

enum log_t   {LOG_LEVEL_NONE, LOG_LEVEL_ERROR, LOG_LEVEL_INFO, LOG_LEVEL_DEBUG, LOG_LEVEL_DEBUG_MORE, LOG_LEVEL_ALL};
enum week_t  {Last, First, Second, Third, Fourth};
enum dow_t   {Sun=1, Mon, Tue, Wed, Thu, Fri, Sat};
enum month_t {Jan=1, Feb, Mar, Apr, May, Jun, Jul, Aug, Sep, Oct, Nov, Dec};
enum wifi_t  {WIFI_RESTART, WIFI_SMARTCONFIG, WIFI_MANAGER, WIFI_WPSCONFIG};
enum swtch_t {TOGGLE, FOLLOW, FOLLOW_INV, PUSHBUTTON, PUSHBUTTON_INV, MAX_SWITCH_OPTION};

#include "user_config.h"

/*********************************************************************************************\
 * Enable feature by removing leading // or disable feature by adding leading //
\*********************************************************************************************/

#define USE_SPIFFS                          // Switch persistent configuration from flash to spiffs (+24k code, +0.6k mem)


/*********************************************************************************************\
 * No user configurable items below
\*********************************************************************************************/

#ifndef SWITCH_MODE
#define SWITCH_MODE            TOGGLE       // TOGGLE, FOLLOW or FOLLOW_INV (the wall switch state)
#endif

#ifndef MQTT_FINGERPRINT
#define MQTT_FINGERPRINT       "A5 02 FF 13 99 9F 8B 39 8E F1 83 4F 11 23 65 0B 32 36 FC 07"
#endif

#define DEF_WIFI_HOSTNAME      "%s-%04d"    // Expands to <MQTT_TOPIC>-<last 4 decimal chars of MAC address>

#define MQTT_UNITS             0            // Default do not show value units (Hr, Sec, V, A, W etc.)
#define MQTT_SUBTOPIC          "LIGHT"      // Default MQTT subtopic (POWER or LIGHT)
#define MQTT_RETRY_SECS        10           // Seconds to retry MQTT connection

#define STATES                 10           // loops per second

#define INPUT_BUFFER_SIZE      100          // Max number of characters in serial buffer
#define TOPSZ                  60           // Max number of characters in topic string
#define MESSZ                  240          // Max number of characters in JSON message string
#define LOGSZ                  128          // Max number of characters in log string
#ifdef USE_MQTT_TLS
  #define MAX_LOG_LINES        10           // Max number of lines in weblog
#else
  #define MAX_LOG_LINES        70           // Max number of lines in weblog
#endif

#define APP_BAUDRATE           115200       // Default serial baudrate

enum butt_t {PRESSED, NOT_PRESSED};

#include "support.h"                        // Global support
#include <Ticker.h>                         // RTC
#include <ESP8266WiFi.h>                    // MQTT, Ota, WifiManager
#include <ESP8266HTTPClient.h>              // MQTT, Ota
#include <ESP8266httpUpdate.h>              // Ota
#include <PubSubClient.h>                   // MQTT
#include <ArduinoJson.h>                    // JSON
#ifdef USE_WEBSERVER
  #include <ESP8266WebServer.h>             // WifiManager, Webserver
  #include <DNSServer.h>                    // WifiManager
#endif  // USE_WEBSERVER
#ifdef USE_SPIFFS
  #include <FS.h>                           // Config
#endif

typedef void (*rtcCallback)();

extern "C" uint32_t _SPIFFS_start;
extern "C" uint32_t _SPIFFS_end;

#define MAX_BUTTON_COMMANDS    5            // Max number of button commands supported

const char commands[MAX_BUTTON_COMMANDS][14] PROGMEM = {
  {"wificonfig 1"},   // Press button three times
  {"wificonfig 2"},   // Press button four times
  {"wificonfig 3"},   // Press button five times
  {"restart 1"},      // Press button six times
  {"upgrade 1"}};     // Press button seven times

const char wificfg[4][12] PROGMEM = { "Restart", "Smartconfig", "Wifimanager", "WPSconfig" };

struct SYSCFG {
  unsigned long cfg_holder;
  unsigned long saveFlag;
  unsigned long version;
  unsigned long bootcount;
  int16_t       savedata;
  byte          savestate;
  int8_t        timezone;
  char          otaUrl[101];
  char          friendlyname[33];

  byte          serial_enable;
  byte          seriallog_level;
  uint8_t       sta_config;
  byte          sta_active;
  char          sta_ssid[2][33];
  char          sta_pwd[2][65];
  char          hostname[33];
  uint8_t       webserver;
  byte          weblog_level;

  char          mqtt_fingerprint[60];
  char          mqtt_host[33];
  uint16_t      mqtt_port;
  char          mqtt_client[33];
  char          mqtt_user[33];
  char          mqtt_pwd[33];
  char          mqtt_topic[33];
  char          mqtt_topic2[33];
  char          mqtt_subtopic[33];
  byte          mqtt_button_retain;
  byte          mqtt_power_retain;
  byte          mqtt_units;
  uint16_t      tele_period;
  uint16_t      sleep_time;

  uint8_t       power;
  uint8_t       ledstate;
  uint8_t       switchmode;

} sysCfg;


struct TIME_T {
  uint8_t       Second;
  uint8_t       Minute;
  uint8_t       Hour;
  uint8_t       Wday;      // day of week, sunday is day 1
  uint8_t       Day;
  uint8_t       Month;
  char          MonthName[4];
  uint16_t      DayOfYear;
  uint16_t      Year;
  unsigned long Valid;
} rtcTime;

struct TimeChangeRule
{
  uint8_t       week;      // 1=First, 2=Second, 3=Third, 4=Fourth, or 0=Last week of the month
  uint8_t       dow;       // day of week, 1=Sun, 2=Mon, ... 7=Sat
  uint8_t       month;     // 1=Jan, 2=Feb, ... 12=Dec
  uint8_t       hour;      // 0-23
  int           offset;    // offset from UTC in minutes
};

TimeChangeRule myDST = { TIME_DST };  // Daylight Saving Time
TimeChangeRule mySTD = { TIME_STD };  // Standard Time

int Baudrate = APP_BAUDRATE;          // Serial interface baud rate
byte SerialInByte;                    // Received byte
int SerialInByteCounter = 0;          // Index in receive buffer
char serialInBuf[INPUT_BUFFER_SIZE + 2];  // Receive buffer
byte Hexcode = 0;                     // Sonoff dual input flag
int16_t savedatacounter;              // Counter and flag for config save to Flash or Spiffs
char Version[16];                     // Version string from VERSION define
char Hostname[33];                    // Composed Wifi hostname
char MQTTClient[33];                  // Composed MQTT Clientname
uint8_t mqttcounter = 0;              // MQTT connection retry counter
unsigned long timerxs = 0;            // State loop timer
int state = 0;                        // State per second flag
int mqttflag = 2;                     // MQTT connection messages flag
int otaflag = 0;                      // OTA state flag
int otaok;                            // OTA result
int restartflag = 0;                  // Sonoff restart flag
int wificheckflag = WIFI_RESTART;     // Wifi state flag
int uptime = 0;                       // Current uptime in hours
int tele_period = 0;                  // Tele period timer
String Log[MAX_LOG_LINES];            // Web log buffer
byte logidx = 0;                      // Index in Web log buffer
int status_update_timer = 0;          // Refresh initial status

#ifdef USE_MQTT_TLS
  WiFiClientSecure espClient;         // Wifi Secure Client
#else
  WiFiClient espClient;               // Wifi Client
#endif
PubSubClient mqttClient(espClient);   // MQTT Client

int blinks = 1;                       // Number of LED blinks
uint8_t blinkstate = 0;               // LED state

uint8_t lastbutton = NOT_PRESSED;     // Last button state
uint8_t holdcount = 0;                // Timer recording button hold
uint8_t multiwindow = 0;              // Max time between button presses to record press count
uint8_t multipress = 0;               // Number of button presses within multiwindow
uint8_t lastbutton2 = NOT_PRESSED;    // Last button 2 state

/********************************************************************************************/

void CFG_Default()
{
  addLog_P(LOG_LEVEL_NONE, PSTR("Config: Use default configuration"));
  memset(&sysCfg, 0x00, sizeof(SYSCFG));

  sysCfg.cfg_holder = CFG_HOLDER;
  sysCfg.saveFlag = 0;
  sysCfg.version = VERSION;
  sysCfg.bootcount = 0;
  sysCfg.savedata = SAVE_DATA;
  sysCfg.savestate = SAVE_STATE;
  sysCfg.timezone = APP_TIMEZONE;
  strlcpy(sysCfg.otaUrl, OTA_URL, sizeof(sysCfg.otaUrl));
  strlcpy(sysCfg.friendlyname, MQTT_TOPIC, sizeof(sysCfg.friendlyname));

  sysCfg.seriallog_level = SERIAL_LOG_LEVEL;
  sysCfg.sta_active = 0;
  strlcpy(sysCfg.sta_ssid[0], STA_SSID1, sizeof(sysCfg.sta_ssid[0]));
  strlcpy(sysCfg.sta_pwd[0], STA_PASS1, sizeof(sysCfg.sta_pwd[0]));
  strlcpy(sysCfg.sta_ssid[1], STA_SSID2, sizeof(sysCfg.sta_ssid[1]));
  strlcpy(sysCfg.sta_pwd[1], STA_PASS2, sizeof(sysCfg.sta_pwd[1]));
  strlcpy(sysCfg.hostname, WIFI_HOSTNAME, sizeof(sysCfg.hostname));
  sysCfg.sta_config = WIFI_CONFIG_TOOL;
   sysCfg.webserver = WEB_SERVER;
  sysCfg.weblog_level = WEB_LOG_LEVEL;

  strlcpy(sysCfg.mqtt_fingerprint, MQTT_FINGERPRINT, sizeof(sysCfg.mqtt_fingerprint));
  strlcpy(sysCfg.mqtt_host, MQTT_HOST, sizeof(sysCfg.mqtt_host));
  sysCfg.mqtt_port = MQTT_PORT;
  strlcpy(sysCfg.mqtt_client, MQTT_CLIENT_ID, sizeof(sysCfg.mqtt_client));
  strlcpy(sysCfg.mqtt_user, MQTT_USER, sizeof(sysCfg.mqtt_user));
  strlcpy(sysCfg.mqtt_pwd, MQTT_PASS, sizeof(sysCfg.mqtt_pwd));
  strlcpy(sysCfg.mqtt_topic, MQTT_TOPIC, sizeof(sysCfg.mqtt_topic));
  strlcpy(sysCfg.mqtt_topic2, "0", sizeof(sysCfg.mqtt_topic2));
  strlcpy(sysCfg.mqtt_subtopic, MQTT_SUBTOPIC, sizeof(sysCfg.mqtt_subtopic));
  sysCfg.mqtt_button_retain = MQTT_BUTTON_RETAIN;
  sysCfg.mqtt_units = MQTT_UNITS;
  sysCfg.tele_period = TELE_PERIOD;
  sysCfg.sleep_time = SLEEP_TIME;


  sysCfg.ledstate = APP_LEDSTATE;
  sysCfg.switchmode = SWITCH_MODE;

  CFG_Save();
}

void CFG_Delta()
{
  if (sysCfg.version != VERSION) {      // Fix version dependent changes

    sysCfg.version = VERSION;
  }
}

/********************************************************************************************/

void getClient(char* output, const char* input, byte size)
{
  char *token;
  uint8_t digits = 0;

  if (strstr(input, "%")) {
    strlcpy(output, input, size);
    token = strtok(output, "%");
    token = strtok(NULL, "");
    if (token != NULL) {
      digits = atoi(token);
      if (digits) {
        snprintf_P(output, size, PSTR("%s%c0%dX"), output, '%', digits);
        snprintf_P(output, size, output, ESP.getChipId());
      }
    }
  }
  if (!digits) strlcpy(output, input, size);
}

void json2legacy(char* stopic, char* svalue)
{
  char *p, *token;
  uint16_t i, j;

  if (!strstr(svalue, "{\"")) return;  // No JSON

  token = strtok(svalue, "{\"");      // Topic
  p = strrchr(stopic, '/') +1;
  i = p - stopic;
  for (j = 0; j < strlen(token)+1; j++) stopic[i+j] = toupper(token[j]);
  token = strtok(NULL, "\"");         // : or :3} or :3, or :{
  if (strstr(token, ":{")) {
    token = strtok(NULL, "\"");       // Subtopic
    token = strtok(NULL, "\"");       // : or :3} or :3,
  }
  if (strlen(token) > 1) {
    token++;
    p = strchr(token, ',');
    if (!p) p = strchr(token, '}');
    i = p - token;
    token[i] = '\0';                  // Value
  } else {
    token = strtok(NULL, "\"");       // Value or , or }
    if ((token[0] == ',') || (token[0] == '}')) {  // Empty parameter
      token = NULL;
    }
  }
  if (token == NULL) {
    svalue[0] = '\0';
  } else {
    memcpy(svalue, token, strlen(token)+1);
  }
}


/********************************************************************************************/

void mqtt_publish(const char* topic, const char* data, boolean retained)
{
  char log[TOPSZ+MESSZ];

  if (mqttClient.publish(topic, data, retained)) {
    snprintf_P(log, sizeof(log), PSTR("MQTT: %s = %s%s"), topic, data, (retained) ? " (retained)" : "");
//    mqttClient.loop();  // Do not use here! Will block previous publishes
  } else {
    snprintf_P(log, sizeof(log), PSTR("RSLT: %s = %s"), topic, data);
  }
  addLog(LOG_LEVEL_INFO, log);
  blinks++;
}

void mqtt_publish(const char* topic, const char* data)
{
  mqtt_publish(topic, data, false);
}



void mqtt_connected()
{
  char stopic[TOPSZ], svalue[MESSZ];

  snprintf_P(stopic, sizeof(stopic), PSTR("%s/%s/#"), SUB_PREFIX, sysCfg.mqtt_topic);
  mqttClient.subscribe(stopic);
  mqttClient.loop();  // Solve LmacRxBlk:1 messages
  snprintf_P(stopic, sizeof(stopic), PSTR("%s/%s/#"), SUB_PREFIX, MQTTClient); // Fall back topic
  mqttClient.subscribe(stopic);
  mqttClient.loop();  // Solve LmacRxBlk:1 messages


  if (mqttflag) {

    //snprintf_P(stopic, sizeof(stopic), PSTR("%s/%s/RESULT"), PUB_PREFIX, sysCfg.mqtt_topic);
    //snprintf_P(svalue, sizeof(svalue), PSTR("{\"Info1\":{\"AppName\":\"" APP_NAME "\", \"Version\":\"%s\", \"FallbackTopic\":\"%s\",}}"),
    //  Version, MQTTClient);
    //mqtt_publish(stopic, svalue);

    // we are connected , measure things and push it to broker
    measure_inputs();

#ifdef USE_WEBSERVER
    if (sysCfg.webserver) {
      snprintf_P(svalue, sizeof(svalue), PSTR("{\"Info2\":{\"WebserverMode\":\"%s\", \"Hostname\":\"%s\", \"IPaddress\":\"%s\"}}"),
        (sysCfg.webserver == 2) ? "Admin" : "User", Hostname, WiFi.localIP().toString().c_str());
      mqtt_publish(stopic, svalue);
    }
#endif  // USE_WEBSERVER
    if (MQTT_MAX_PACKET_SIZE < (TOPSZ+MESSZ)) {
      snprintf_P(svalue, sizeof(svalue), PSTR("{\"Warning1\":\"Change MQTT_MAX_PACKET_SIZE in libraries/PubSubClient.h to at least %d\"}"), TOPSZ+MESSZ);

      mqtt_publish(stopic, svalue);
    }
    if (!spiffsPresent()) {
      snprintf_P(svalue, sizeof(svalue), PSTR("{\"Warning2\":\"No persistent config. Please reflash with at least 16K SPIFFS\"}"));

      mqtt_publish(stopic, svalue);
    }
    if (sysCfg.tele_period) tele_period = sysCfg.tele_period -9;
    status_update_timer = 2;
  }
  mqttflag = 0;
}

void mqtt_reconnect()
{
  char stopic[TOPSZ], svalue[TOPSZ], log[LOGSZ];

  mqttcounter = MQTT_RETRY_SECS;

  if (mqttflag > 1) {
#ifdef USE_MQTT_TLS
    addLog_P(LOG_LEVEL_INFO, PSTR("MQTT: Verify TLS fingerprint..."));
    if (!espClient.connect(sysCfg.mqtt_host, sysCfg.mqtt_port)) {
      snprintf_P(log, sizeof(log), PSTR("MQTT: TLS CONNECT FAILED USING WRONG MQTTHost (%s) or MQTTPort (%d). Retry in %d seconds"),
        sysCfg.mqtt_host, sysCfg.mqtt_port, mqttcounter);
      addLog(LOG_LEVEL_DEBUG, log);
      return;
    }
    if (espClient.verify(sysCfg.mqtt_fingerprint, sysCfg.mqtt_host)) {
      addLog_P(LOG_LEVEL_INFO, PSTR("MQTT: Verified"));
    } else {
      addLog_P(LOG_LEVEL_DEBUG, PSTR("MQTT: WARNING - Insecure connection due to invalid Fingerprint"));
    }
#endif  // USE_MQTT_TLS
    mqttClient.setServer(sysCfg.mqtt_host, sysCfg.mqtt_port);
    mqttClient.setCallback(mqttDataCb);
    mqttflag = 1;
    mqttcounter = 1;
    return;
  }

  addLog_P(LOG_LEVEL_INFO, PSTR("MQTT: Attempting connection..."));
  snprintf_P(stopic, sizeof(stopic), PSTR("%s/%s/TELEMETRY"), PUB_PREFIX2, sysCfg.mqtt_topic);
  snprintf_P(svalue, sizeof(svalue), PSTR("{\"LWT\":\"Offline\"}"));

  if (mqttClient.connect(MQTTClient, sysCfg.mqtt_user, sysCfg.mqtt_pwd, stopic, 0, 0, svalue)) {
    addLog_P(LOG_LEVEL_INFO, PSTR("MQTT: Connected"));
    mqttcounter = 0;
    mqtt_connected();
  } else {
    snprintf_P(log, sizeof(log), PSTR("MQTT: CONNECT FAILED, rc %d. Retry in %d seconds"), mqttClient.state(), mqttcounter);
    addLog(LOG_LEVEL_DEBUG, log);
  }
}

void mqttDataCb(char* topic, byte* data, unsigned int data_len)
{
  uint16_t i = 0, grpflg = 0, index;
  char topicBuf[TOPSZ], dataBuf[data_len+1], dataBufUc[MESSZ];
  char *str, *p, *mtopic = NULL, *type = NULL, *devc = NULL;
  char stopic[TOPSZ], svalue[MESSZ], stemp1[TOPSZ], stemp2[10], stemp3[10];
  float ped, pi, pc;
  uint16_t pe, pw, pu;

  strncpy(topicBuf, topic, sizeof(topicBuf));
  memcpy(dataBuf, data, sizeof(dataBuf));
  dataBuf[sizeof(dataBuf)-1] = 0;

  snprintf_P(svalue, sizeof(svalue), PSTR("MQTT: Receive topic %s, data size %d, data %s"), topicBuf, data_len, dataBuf);
  addLog(LOG_LEVEL_DEBUG_MORE, svalue);

  memmove(topicBuf, topicBuf+sizeof(SUB_PREFIX), sizeof(topicBuf)-sizeof(SUB_PREFIX));  // Remove SUB_PREFIX

  i = 0;
  for (str = strtok_r(topicBuf, "/", &p); str && i < 3; str = strtok_r(NULL, "/", &p)) {
    switch (i++) {
    case 0:  // Topic / GroupTopic / DVES_123456
      mtopic = str;
      break;
    case 1:  // TopicIndex / Text
      type = str;
      break;
    case 2:  // Text
      devc = str;
    }
  }

  if (type != NULL) {
    for(i = 0; i < strlen(type); i++) type[i] = toupper(type[i]);
    i--;
    /*if ((type[i] > '0') && (type[i] < '9')) {
      type[i] = '\0';
    }*/
  }

  for(i = 0; i <= sizeof(dataBufUc); i++) dataBufUc[i] = toupper(dataBuf[i]);

  snprintf_P(svalue, sizeof(svalue), PSTR("MQTT: DataCb Topic %s,  Type %s, Data %s (%s)"),
    mtopic, type, dataBuf, dataBufUc);
  addLog(LOG_LEVEL_DEBUG, svalue);

  // sample message
  // DataCb Topic NocnaLampa,  Type RGB, Data {"brightness": 255,"color": {"g": 255,"b": 255,"r": 255},"transition": 2,"state": "ON"} ({"BRIGHTNESS": 255,"COLOR": {"G": 255,"B": 255,"R": 255},"TRANSITION": 2,"STATE": "ON"})

  /*
  if (!strcmp(type,"RGB") || !strcmp(type,"LED1") || !strcmp(type,"LED2") ) {
    if ((data_len > 0))  {
      // process JSON
      StaticJsonBuffer<MESSZ> jsonBuffer;

      JsonObject& root = jsonBuffer.parseObject(data);

      if (!root.success()) {
        snprintf_P(svalue, sizeof(svalue), PSTR("Failed to parse JSON"));
        addLog(LOG_LEVEL_DEBUG, svalue);
      }


      if (root.containsKey("state")) {
        if (strcmp(root["state"], CMND_ON) == 0) {
          stateOn = true;
        }
        else if (strcmp(root["state"], CMND_OFF) == 0) {
          stateOn = false;
        }
      }


      snprintf_P(stopic, sizeof(stopic), PSTR("%s/%s/%s"), PUB_PREFIX, sysCfg.mqtt_topic, type );



    }

    //    system callbacks

  } else */ if (!strcmp(type,"UPGRADE") || !strcmp(type,"UPLOAD")) {
      if (data_len > 0) {
        otaflag = 3;
        snprintf_P(svalue, sizeof(svalue), PSTR("{\"Upgrade\":\"Version %s from %s\"}"), Version, sysCfg.otaUrl);
      } else {
        snprintf_P(svalue, sizeof(svalue), PSTR("{\"Upgrade\":\"Option 1 to upgrade\"}"));
      }
    }
    else if (!strcmp(type,"RESTART")) {
      restartflag = 2;
      snprintf_P(svalue, sizeof(svalue), PSTR("{\"Restart\":\"Restarting\"}"));
    }
   else if (type != NULL) {
    snprintf_P(stopic, sizeof(stopic), PSTR("%s/%s/RESULT"), PUB_PREFIX, sysCfg.mqtt_topic);
    snprintf_P(svalue, sizeof(svalue), PSTR("{\"Command\":\"Error\"}"));
  }

  mqtt_publish(stopic, svalue);
}

/********************************************************************************************/

void measure_inputs() {
  char stopic[TOPSZ], svalue[TOPSZ], log[LOGSZ];
  unsigned int  measurement0, measurementX , humidity;

  // Before measurement we set EN_PIN to high, which will provide power for 3.3 V rail
  digitalWrite(EN_PIN, 1) ;
  delay(500);

  // channel 0 is for battery measuring
  // voltage divider across battery and using internal ADC ref voltage of 1V
  // Sense point is bypassed with 0.1 uF cap to reduce noise at that point
  // 708 k ohm a 148.9 k ohm , ref 1V Vmax at battery 5.755V , use http://www.raltron.com/cust/tools/voltage_divider.asp for max V calculation
  // 5.755/1023 = Volts per bit = 0,0066579292267366

  measurement0 = muxInput(0);
  float battery_reading = measurement0 * 0.0066579292267366 ;
  char battery_voltage[4];
  dtostrf(battery_reading, 4, 2, battery_voltage);

      snprintf_P(stopic, sizeof(stopic), PSTR("%s/%s/0"), PUB_PREFIX, sysCfg.mqtt_topic  );
      snprintf_P(svalue, sizeof(svalue), PSTR("{\"battery_voltage\":\"%s\"}"), battery_voltage );
      mqtt_publish(stopic, svalue);
  //delay(5000);

  // channel 1 - 8 are for humidity readings
  // max voltage on output is 3.3 V which means 0 % humidity
  // with our calcuation of voltage divider 3.3 V represents 465 bits from ADC
  for (size_t i = 1; i <= 8; ){
    measurementX = muxInput(i);
    humidity = 100 - ( measurementX / 4.65 );
    if (humidity <= 0 ) { humidity = 0 ; } else if (humidity > 100 ) { humidity = 100 ;} ;
        snprintf_P(stopic, sizeof(stopic), PSTR("%s/%s/%d"), PUB_PREFIX, sysCfg.mqtt_topic, i  );
        snprintf_P(svalue, sizeof(svalue), PSTR("{\"humidity\":\"%d\"}"), humidity );
        mqtt_publish(stopic, svalue);
    //delay(1000);
    i++ ;
  }

  // After measurement we switch off the EN_PIN for power saving.
  digitalWrite(EN_PIN, 0) ;

  snprintf_P(log, sizeof(log), PSTR("APP: Going to sleep for %d seconds / %d minutes"), sysCfg.sleep_time, sysCfg.sleep_time / 60);
  addLog(LOG_LEVEL_DEBUG, log);

  // end of measurement, put esp to sleep
  ESP.deepSleep( (sysCfg.sleep_time * 1000000) ) ;
}


int muxInput(unsigned int MuxChannel) {
  unsigned int S0, S1, S2, S3 , r , current_channel; char log[LOGSZ];

  current_channel = MuxChannel;

  S0 = MuxChannel & 0x01; digitalWrite(MUX_S0_PIN, S0); // I only want the last bit
  S1 = (MuxChannel >>= 1) & 0x01; digitalWrite(MUX_S1_PIN, S1);
  S2 = (MuxChannel >>= 1) & 0x01; digitalWrite(MUX_S2_PIN, S2);
  S3 = (MuxChannel >>= 1) & 0x01 ; digitalWrite(MUX_S3_PIN, S3);

  snprintf_P(log, sizeof(log), PSTR("APP: Muxing to desired channel %d. Pins 0 1 2 3 - %d / %d / %d / %d"),
  current_channel , S0, S1, S2, S3);
  addLog(LOG_LEVEL_DEBUG_MORE, log);

  // we addressed multiplexer, we can read values
  r = analogRead(A0);
  return r;
}


void do_cmnd(char *cmnd)
{
  char stopic[TOPSZ], svalue[128];
  char *token;

  token = strtok(cmnd, " ");
  snprintf_P(stopic, sizeof(stopic), PSTR("%s/%s/%s"), SUB_PREFIX, sysCfg.mqtt_topic, token);
  token = strtok(NULL, "");
  snprintf_P(svalue, sizeof(svalue), PSTR("%s"), (token == NULL) ? "" : token);
  mqttDataCb(stopic, (byte*)svalue, strlen(svalue));
}



void every_second_cb()
{
  // 1 second rtc interrupt routine
  // Keep this code small (every_second is to large - it'll trip exception)
}

void every_second()
{
  char log[LOGSZ], stopic[TOPSZ], svalue[MESSZ], stime[21], stemp0[10], stemp1[10], stemp2[10], stemp3[10];
  uint8_t i, djson;
  float t, h, ped, pi, pc;
  uint16_t pe, pw, pu;

  snprintf_P(stime, sizeof(stime), PSTR("%04d-%02d-%02dT%02d:%02d:%02d"),
    rtcTime.Year, rtcTime.Month, rtcTime.Day, rtcTime.Hour, rtcTime.Minute, rtcTime.Second);

  if (status_update_timer) {
    status_update_timer--;
    if (!status_update_timer) {
      // mqtt publish states of all lights
    }
  }

  if (sysCfg.tele_period) { // send TELEMETRY data
    tele_period++;
    if (tele_period == sysCfg.tele_period -1) {

    }
    if (tele_period >= sysCfg.tele_period) {
      tele_period = 0;

      snprintf_P(stopic, sizeof(stopic), PSTR("%s/%s/TELEMETRY"), PUB_PREFIX2, sysCfg.mqtt_topic);
      snprintf_P(svalue, sizeof(svalue), PSTR("{\"Time\":\"%s\", \"Uptime\":%d"), stime, uptime);
      // mqtt send light state of all lights
      snprintf_P(svalue, sizeof(svalue), PSTR("%s, \"Wifi\":{\"AP\":%d, \"SSID\":\"%s\", \"RSSI\":%d}}"),
        svalue, sysCfg.sta_active +1, sysCfg.sta_ssid[sysCfg.sta_active], WIFI_getRSSIasQuality(WiFi.RSSI()));
      mqtt_publish(stopic, svalue);

      djson = 0;
      snprintf_P(svalue, sizeof(svalue), PSTR("{\"Time\":\"%s\""), stime);


      if (djson) {
        snprintf_P(svalue, sizeof(svalue), PSTR("%s}"), svalue);
        mqtt_publish(stopic, svalue);
      }

    }
  }


  if ((rtcTime.Minute == 2) && (rtcTime.Second == 30)) {
    uptime++;

    snprintf_P(stopic, sizeof(stopic), PSTR("%s/%s/TELEMETRY"), PUB_PREFIX2, sysCfg.mqtt_topic);
    snprintf_P(svalue, sizeof(svalue), PSTR("{\"Time\":\"%s\", \"Uptime\":%d}"), stime, uptime);

    mqtt_publish(stopic, svalue);
  }
}


void stateloop()
{
  uint8_t button, flag;
  char scmnd[20], log[LOGSZ], stopic[TOPSZ], svalue[TOPSZ];

  timerxs = millis() + (1000 / STATES);
  state++;
  if (state == STATES) {             // Every second
    state = 0;
    every_second();
  }

  //measure_inputs();

  #ifdef W1_BUTTON_PIN

    button = digitalRead(W1_BUTTON_PIN);

    if ((button == PRESSED) && (lastbutton == NOT_PRESSED)) {
      multipress = (multiwindow) ? multipress +1 : 1;
      snprintf_P(log, sizeof(log), PSTR("APP: Multipress 1 button %d"), multipress);
      addLog(LOG_LEVEL_DEBUG, log);
      blinks = 1;
      multiwindow = STATES /2;         // 1/2 second multi press window
    }
    lastbutton = button;
    if (button == NOT_PRESSED) {
      holdcount = 0;
    } else {
      holdcount++;
      if (holdcount == (STATES *2)) {  // 2 seconds button hold
        // fade out all lights
        snprintf_P(log, sizeof(log), PSTR("APP: 2 seconds hold button 1 ") );
        addLog(LOG_LEVEL_DEBUG, log);

        // send_button_state("RGB",0);
        multipress = 0;
      }
      else if (holdcount == (STATES *8)) {  // 8 seconds button hold
        snprintf_P(scmnd, sizeof(scmnd), PSTR("reset 1"));
        multipress = 0;
        do_cmnd(scmnd);
      }
    }
  #endif // End W1_BUTTON_PIN


  if (multiwindow) {
    multiwindow--;
  } else {
    if ((!restartflag) && (!holdcount) && (multipress > 0) && (multipress < MAX_BUTTON_COMMANDS +3)) {
        flag = (multipress == 1);
      if (flag && mqttClient.connected()) {
          // Execute command via MQTT using ButtonTopic to sync external clients


      } else {
        if ((multipress == 1) || (multipress == 2)) {
          if (WIFI_State()) {  // WPSconfig, Smartconfig or Wifimanager active
            restartflag = 1;
          } else {
            // Execute command internally

          }
        } else {
          snprintf_P(scmnd, sizeof(scmnd), commands[multipress -3]);
          do_cmnd(scmnd);
        }
      }
      multipress = 0;
    }
  }


  if (!(state % ((STATES/10)*2))) {
    if (blinks || restartflag || otaflag) {
      if (restartflag || otaflag) {
        blinkstate = 1;   // Stay lit
      } else {
        blinkstate ^= 1;  // Blink
      }
      digitalWrite(LED_PIN, (LED_INVERTED) ? !blinkstate : blinkstate);
      if (!blinkstate) blinks--;
    } else {
    }
  }

  switch (state) {
  case (STATES/10)*2:
    if (otaflag) {
      otaflag--;
      if (otaflag <= 0) {
        otaflag = 12;
        ESPhttpUpdate.rebootOnUpdate(false);
        otaok = (ESPhttpUpdate.update(sysCfg.otaUrl) == HTTP_UPDATE_OK);
      }
      if (otaflag == 10) {  // Allow MQTT to reconnect
        otaflag = 0;
        snprintf_P(stopic, sizeof(stopic), PSTR("%s/%s/UPGRADE"), PUB_PREFIX, sysCfg.mqtt_topic);
        if (otaok) {
          snprintf_P(svalue, sizeof(svalue), PSTR("Successful. Restarting"));
          restartflag = 2;
        } else {
          snprintf_P(svalue, sizeof(svalue), PSTR("Failed %s"), ESPhttpUpdate.getLastErrorString().c_str());
        }
        mqtt_publish(stopic, svalue);
      }
    }
    break;
  case (STATES/10)*4:
    if (savedatacounter) {
      savedatacounter--;
      if (savedatacounter <= 0) {
      if (sysCfg.savestate) {

      }
        CFG_Save();
        savedatacounter = sysCfg.savedata;
      }
    }
    if (restartflag) {
      if (restartflag == 211) {
        CFG_Default();
        restartflag = 2;
      }
      if (restartflag == 212) {
        CFG_Erase();
        CFG_Default();
        restartflag = 2;
      }
      if (sysCfg.savestate) {

      }

      CFG_Save();
      restartflag--;
      if (restartflag <= 0) {
        addLog_P(LOG_LEVEL_INFO, PSTR("APP: Restarting"));
        ESP.restart();
      }
    }
    break;
  case (STATES/10)*6:
    WIFI_Check(wificheckflag);
    wificheckflag = WIFI_RESTART;
    break;
  case (STATES/10)*8:
    if ((WiFi.status() == WL_CONNECTED) && (!mqttClient.connected())) {
      if (!mqttcounter) {
        mqtt_reconnect();
      } else {
        mqttcounter--;
      }
    }
    break;
  }

}


/********************************************************************************************/

void setup()
{
  char log[LOGSZ];
  byte idx;

  Serial.begin(Baudrate);
  delay(10);
  Serial.println();
  sysCfg.seriallog_level = LOG_LEVEL_INFO;  // Allow specific serial messages until config loaded

  snprintf_P(Version, sizeof(Version), PSTR("%d.%d.%d"), VERSION >> 24 & 0xff, VERSION >> 16 & 0xff, VERSION >> 8 & 0xff);
  if (VERSION & 0x1f) {
    idx = strlen(Version);
    Version[idx] = 96 + (VERSION & 0x1f);
    Version[idx +1] = 0;
  }
  if (!spiffsPresent())
    addLog_P(LOG_LEVEL_ERROR, PSTR("SPIFFS: ERROR - No spiffs present. Please reflash with at least 16K SPIFFS"));
#ifdef USE_SPIFFS
  initSpiffs();

#endif
  CFG_Load();

  CFG_Delta();


  if (Serial.baudRate() != Baudrate) {
    snprintf_P(log, sizeof(log), PSTR("APP: Need to change baudrate to %d"), Baudrate);
    addLog(LOG_LEVEL_INFO, log);
    delay(100);
    Serial.flush();
    Serial.begin(Baudrate);
    delay(10);
    Serial.println();
  }

  sysCfg.bootcount++;
  snprintf_P(log, sizeof(log), PSTR("APP: Bootcount %d"), sysCfg.bootcount);
  addLog(LOG_LEVEL_DEBUG, log);
  savedatacounter = sysCfg.savedata;


  if (strstr(sysCfg.hostname, "%")) strlcpy(sysCfg.hostname, DEF_WIFI_HOSTNAME, sizeof(sysCfg.hostname));
  if (!strcmp(sysCfg.hostname, DEF_WIFI_HOSTNAME)) {
    snprintf_P(Hostname, sizeof(Hostname)-1, sysCfg.hostname, sysCfg.mqtt_topic, ESP.getChipId() & 0x1FFF);
  } else {
    snprintf_P(Hostname, sizeof(Hostname)-1, sysCfg.hostname);
  }


  WIFI_Connect(Hostname);

  getClient(MQTTClient, sysCfg.mqtt_client, sizeof(MQTTClient));

  pinMode(LED_PIN, OUTPUT);
  digitalWrite(LED_PIN, (LED_INVERTED) ? !blinkstate : blinkstate);

  pinMode(MUX_S0_PIN, OUTPUT);
  pinMode(MUX_S1_PIN, OUTPUT);
  pinMode(MUX_S2_PIN, OUTPUT);
  pinMode(MUX_S3_PIN, OUTPUT);
  pinMode(EN_PIN, OUTPUT);

  rtc_init(every_second_cb);

  snprintf_P(log, sizeof(log), PSTR("APP: Project %s (Topic %s, Fallback %s, ) Version %s"),
    PROJECT, sysCfg.mqtt_topic, MQTTClient, Version);
  addLog(LOG_LEVEL_INFO, log);
}

void loop()
{
#ifdef USE_WEBSERVER
  pollDnsWeb();
#endif  // USE_WEBSERVER

  if (millis() >= timerxs) stateloop();

  // keep mqtt connected
  // mqttClient.loop();

  //if (Serial.available()) serial();

  yield();
}
