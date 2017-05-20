/*********************************************************************************************\
 * User specific configuration parameters
 *
 * Corresponding MQTT/Serial/Console commands in [brackets]
\*********************************************************************************************/

// -- Project --------------------------------
#define PROJECT                "PlantMonitor"     // PROJECT is used as the default topic delimiter and OTA file name
                                            //   As an IDE restriction it needs to be the same as the main .ino file

#define CFG_HOLDER             0x20161322   // [Reset 1] Change this value to load following default configuration parameters
#define SAVE_DATA              0            // [SaveData] Save changed parameters to Flash (0 = disable, 1 - 3600 seconds)
#define SAVE_STATE             0            // [SaveState] Save changed power state to Flash (0 = disable, 1 = enable)

// -- Wifi -----------------------------------
#define STA_SSID1              "Your-IoT-SSID"       // [Ssid1] Wifi SSID
#define STA_PASS1              "SecretPass"   // [Password1] Wifi password
#define STA_SSID2              ""      // [Ssid2] Optional alternate AP Wifi SSID
#define STA_PASS2              ""  // [Password2] Optional alternate AP Wifi password
#define WIFI_HOSTNAME          "%s-%04d"         // [Hostname] Expands to <MQTT_TOPIC>-<last 4 decimal chars of MAC address>
#define WIFI_CONFIG_TOOL       WIFI_MANAGER    // [WifiConfig] Default tool if wifi fails to connect (WIFI_RESTART, WIFI_SMARTCONFIG, WIFI_MANAGER or WIFI_WPSCONFIG)

#define SERIAL_LOG_LEVEL       LOG_LEVEL_DEBUG_MORE  // [SerialLog]
#define WEB_LOG_LEVEL          LOG_LEVEL_DEBUG_MORE  // [WebLog]

// -- Ota ------------------------------------
#if (ARDUINO >= 168)
  #define OTA_URL              "http://hostname:80/api/arduino/" PROJECT ".ino.bin"  // [OtaUrl]
#else
  #define OTA_URL              "http://hostname:80/api/arduino/" PROJECT ".cpp.bin"  // [OtaUrl]
#endif

// -- MQTT -----------------------------------
// !!! TLS uses a LOT OF MEMORY (20k) so be careful to enable other options at the same time !!!
//#define USE_MQTT_TLS                        // EXPERIMENTAL Use TLS for MQTT connection (+53k code, +20k mem)
                                            //   Needs Fingerprint, TLS Port, UserId and Password
#ifdef USE_MQTT_TLS
  #define MQTT_HOST            "X.X.X.X"  // [MqttHost]
  #define MQTT_FINGERPRINT     "A5 02 FF 13 99 9F 8B 39 8E F1 83 4F 11 23 65 0B 32 36 FC 07"  // [MqttFingerprint]
  #define MQTT_PORT            20123                // [MqttPort] MQTT TLS port
  #define MQTT_USER            ""      // [MqttUser] Mandatory user
  #define MQTT_PASS            ""  // [MqttPassword] Mandatory password
#else
  #define MQTT_HOST            "X.X.X.X"     // [MqttHost]
  #define MQTT_PORT            1883         // [MqttPort] MQTT port (10123 on CloudMQTT)
  #define MQTT_USER            "mqtt_user"  // [MqttUser] Optional user
  #define MQTT_PASS            "mqtt_pass"  // [MqttPassword] Optional password
#endif

#define MQTT_CLIENT_ID         "DVES_%06X"  // [MqttClient] Also fall back topic using Chip Id = last 6 characters of MAC address

#define SUB_PREFIX             "cmnd"       // Sonoff devices subscribe to:- SUB_PREFIX/MQTT_TOPIC
#define PUB_PREFIX             "stat"       // Sonoff devices publish to:- PUB_PREFIX/MQTT_TOPIC
#define PUB_PREFIX2            "stat"       // Sonoff devices publish telemetry data to:- PUB_PREFIX2/MQTT_TOPIC/UPTIME, POWER/LIGHT and TIME
                                            //   May be named the same as PUB_PREFIX
#define MQTT_TOPIC             PROJECT      // [Topic] (unique) MQTT device topic
#define MQTT_BUTTON_RETAIN     1            // [ButtonRetain] Button may send retain flag (0 = off, 1 = on)
#define CMND_ON                "ON"         // Command to turn ON light
#define CMND_OFF               "OFF"        // Command to turn OFF light

// -- MQTT - Telemetry -----------------------
#define TELE_PERIOD            3600           // [TelePeriod] Telemetry (0 = disable, 10 - 3600 seconds)
#define SLEEP_TIME             1800           // [SleepTime] Time to sleep (0 = sleep forever, 1 - X seconds)
#define SEND_TELEMETRY_UPTIME               // Enable sending uptime telemetry (if disabled will still send hourly message)
#define SEND_TELEMETRY_WIFI                 // Enable sending wifi telemetry

// -- HTTP -----------------------------------
//#define USE_WEBSERVER                       // Enable web server and wifi manager (+43k code, +2k mem) - Disable by //

  #define WEB_SERVER           2            // [WebServer] Web server (0 = Off, 1 = Start as User, 2 = Start as Admin)

// -- Time - Up to three NTP servers in your region
#define NTP_SERVER1            "X.X.X.X"
#define NTP_SERVER2            "sk.pool.ntp.org"
#define NTP_SERVER3            "0.sk.pool.ntp.org"

// -- Time - Start Daylight Saving Time and timezone offset from UTC in minutes
#define TIME_DST               Last, Sun, Mar, 2, +120  // Last sunday in march at 02:00 +120 minutes

// -- Time - Start Standard Time and timezone offset from UTC in minutes
#define TIME_STD               Last, Sun, Oct, 3, +60   // Last sunday in october 02:00 +60 minutes

// -- Application ----------------------------
#define APP_TIMEZONE           1            // [Timezone] +1 hour (Amsterdam) (-12 .. 12 = hours from UTC, 99 = use TIME_DST/TIME_STD)
#define APP_LEDSTATE           1            // [LedState] Do not show power state (1 = Show power state)

#define APP_NAME             "ESP Plant Monitor"

#define LED_PIN              2           // GPIO 02 = Blue Onboard Led (0 = On, 1 = Off)
#define LED_INVERTED         1           // 0 = (1 = On, 0 = Off), 1 = (0 = On, 1 = Off)
//#define W1_BUTTON_PIN        5           // GPIO Where White1 button is connected (INPUT)
#define MUX_S0_PIN           12          // GPIO Where S0 pin from multiplexer is connected
#define MUX_S1_PIN           13          // GPIO Where S1 pin from multiplexer is connected
#define MUX_S2_PIN           14          // GPIO Where S2 pin from multiplexer is connected
#define MUX_S3_PIN           4          // GPIO Where S3 pin from multiplexer is connected
#define EN_PIN               5          // GPIO Where will be + for 3.3 V power rail

/*********************************************************************************************\
 * No user configurable items below
\*********************************************************************************************/
