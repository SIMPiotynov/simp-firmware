/***************************************************
  Main of FingerprintDoorbell
 ****************************************************/

#include <WiFi.h>
#include <DNSServer.h>
#include <time.h>
#include <ESPAsyncWebServer.h>
#include <HTTPClient.h>
#include <Arduino_JSON.h>
#include <FS.h>
#include <SPIFFS.h>
#include <melody_player.h>

#include "FingerprintManager.h"
#include "SettingsManager.h"
#include "global.h"
#include "player.h"

#define BUZZER_PIN 27

const char* VersionInfo = "0.4";

enum class Mode { scan, enroll, maintenance };

const long  gmtOffset_sec = 0; // UTC Time
const int   daylightOffset_sec = 0; // UTC Time

const int logMessagesCount = 5;
String logMessages[logMessagesCount]; // log messages, 0=most recent log message
bool shouldReboot = false;
unsigned long wifiReconnectPreviousMillis = 0;

int enrollId;
String enrollName;
HTTPClient http;
Mode currentMode = Mode::scan;
MelodyPlayer player(BUZZER_PIN, 0, false);
Melody track;
FingerprintManager fingerManager;
SettingsManager settingsManager;
bool needMaintenanceMode = false;
long lastMsg = 0;
char msg[50];
int value = 0;

Match lastMatch;

void addLogMessage(const String& message) {
  // shift all messages in array by 1, oldest message will die
  for (int i=logMessagesCount-1; i>0; i--)
    logMessages[i]=logMessages[i-1];
  logMessages[0]=message;
}

String getLogMessagesAsHtml() {
  String html = "";
  for (int i=logMessagesCount-1; i>=0; i--) {
    if (logMessages[i]!="")
      html = html + logMessages[i] + "<br>";
  }
  return html;
}

String getTimestampString(){
  struct tm timeinfo;
  if(!getLocalTime(&timeinfo)){
    Serial.println("Failed to obtain time");
    return "no time";
  }

  char buffer[25];
  strftime(buffer,sizeof(buffer),"%Y-%m-%d %H:%M:%S %Z", &timeinfo);
  String datetime = String(buffer);
  return datetime;
}

/* wait for maintenance mode or timeout 5s */
bool waitForMaintenanceMode() {
  needMaintenanceMode = true;
  unsigned long startMillis = millis();
  while (currentMode != Mode::maintenance) {
    if ((millis() - startMillis) >= 5000ul) {
      needMaintenanceMode = false;
      return false;
    }
    delay(50);
  }
  needMaintenanceMode = false;
  return true;
}

// send LastMessage to websocket clients
void notifyClients(String message) {
  String messageWithTimestamp = "[" + getTimestampString() + "]: " + message;
  Serial.println(messageWithTimestamp);
  addLogMessage(messageWithTimestamp);
}

bool doPairing() {
  String newPairingCode = settingsManager.generateNewPairingCode();

  if (fingerManager.setPairingCode(newPairingCode)) {
    AppSettings settings = settingsManager.getAppSettings();
    settings.sensorPairingCode = newPairingCode;
    settings.sensorPairingValid = true;
    settingsManager.saveAppSettings(settings);
    notifyClients("Pairing successful.");
    return true;
  } else {
    notifyClients("Pairing failed.");
    return false;
  }

}

bool checkPairingValid() {
  AppSettings settings = settingsManager.getAppSettings();

   if (!settings.sensorPairingValid) {
     if (settings.sensorPairingCode.isEmpty()) {
       // first boot, do pairing automatically so the user does not have to do this manually
       return doPairing();
     } else {
      Serial.println("Pairing has been invalidated previously.");
      return false;
     }
   }

  String actualSensorPairingCode = fingerManager.getPairingCode();
  //Serial.println("Awaited pairing code: " + settings.sensorPairingCode);
  //Serial.println("Actual pairing code: " + actualSensorPairingCode);

  if (actualSensorPairingCode.equals(settings.sensorPairingCode))
    return true;
  else {
    if (!actualSensorPairingCode.isEmpty()) {
      // An empty code means there was a communication problem. So we don't have a valid code, but maybe next read will succeed and we get one again.
      // But here we just got an non-empty pairing code that was different to the awaited one. So don't expect that will change in future until repairing was done.
      // -> invalidate pairing for security reasons
      AppSettings settings = settingsManager.getAppSettings();
      settings.sensorPairingValid = false;
      settingsManager.saveAppSettings(settings);
    }
    return false;
  }
}

void doScan() {
  Match match = fingerManager.scanFingerprint();

  switch(match.scanResult)
  {
    case ScanResult::noFinger:
      // standard case, occurs every iteration when no finger touchs the sensor
      // Serial.println("no finger");
      // if (match.scanResult != lastMatch.scanResult) {
      //   Serial.println("no finger");
      // }
      break;
    case ScanResult::matchFound:
      notifyClients( String("Match Found: ") + match.matchId + " - " + match.matchName  + " with confidence of " + match.matchConfidence );
      if (match.scanResult != lastMatch.scanResult) {
        if (match.matchId != lastMatch.matchId) {
          if (checkPairingValid()) {
            // Ouvre la porte et sonne
            Serial.println("Open the door!");
            track = getTrackPath("string", "reussi:d=4,o=5,b=250:e,8p,8f,8g,8p,3c6");

            if (track) {
              player.play(track);
            }
          }
        } else {
          // Mode enregistrement
          enrollId = fingerManager.countFingerRegistred() + 1;
          enrollName = "newFingerprintName_" + String(1);
          currentMode = Mode::enroll;
        }
      } else {
        Serial.println("Security issue! invalid sensor pairing! This could potentially be an attack! If the sensor is new or has been replaced by you do a (re)pairing in settings page.");
      }

      lastMatch = match;
      lastMatch.scanResult = ScanResult::noFinger;
      delay(3000); // wait some time before next scan to let the LED blink
      break;
    case ScanResult::noMatchFound:
      notifyClients(String("No Match Found (Code ") + match.returnCode + ")");
      if (match.scanResult != lastMatch.scanResult) {
        track = getTrackPath("file", "goodbad");

        if (track) {
          player.play(track);
        }
        delay(1000);
      } else {
        Serial.println("Not the same finger.");
        delay(1000); // wait some time before next scan to let the LED blink
      }

      lastMatch = match;
      lastMatch.scanResult = ScanResult::noFinger;
      break;
    case ScanResult::error:
      notifyClients(String("ScanResult Error (Code ") + match.returnCode + ")");
      break;
  };
}

void doEnroll() {
  int id = enrollId;
  if (id < 1 || id > 200) {
    notifyClients("Invalid memory slot id '" + String(enrollId) + "'");
    return;
  }

  NewFinger finger = fingerManager.enrollFinger(id, enrollName);
  if (finger.enrollResult == EnrollResult::ok) {
    notifyClients("Enrollment successfull. You can now use your new finger for scanning.");
    fingerManager.setFingersRegistred(enrollId);
  }  else if (finger.enrollResult == EnrollResult::error) {
    notifyClients(String("Enrollment failed. (Code ") + finger.returnCode + ")");
  }
}

bool initWifi() {
  // Connect to Wi-Fi
  WifiSettings wifiSettings = settingsManager.getWifiSettings();
  WiFi.mode(WIFI_STA);
  WiFi.config(INADDR_NONE, INADDR_NONE, INADDR_NONE, INADDR_NONE);
  WiFi.begin(wifiSettings.ssid.c_str(), wifiSettings.password.c_str());
  int counter = 0;
  while (WiFi.status() != WL_CONNECTED) {
    delay(1000);
    Serial.println("Waiting for WiFi connection...");
    counter++;
    if (counter > 30)
      return false;
  }
  Serial.println("Connected!");

  // Print ESP32 Local IP Address
  Serial.println(WiFi.localIP());

  return true;
}

void reboot() {
  notifyClients("System is rebooting now...");
  delay(1000);

  WiFi.disconnect();
  ESP.restart();
}

void doRequest(const char * type, String payload) {
  int httpResponseCode = http.sendRequest(type, payload);

  if(httpResponseCode>0){
    String response = http.getString();

    Serial.println(httpResponseCode);
    Serial.println(response);
  }else{
    Serial.print("Error on sending POST: ");
    Serial.println(httpResponseCode);
  }

  http.end();
}

void setup() {
  // open serial monitor for debug infos
  Serial.begin(115200);
  while (!Serial);  // For Yun/Leo/Micro/Zero/...
  delay(100);

  SPIFFS.begin(true);

  http.begin("http://jsonplaceholder.typicode.com/posts");
  http.addHeader("Content-Type", "text/plain");

  settingsManager.loadWifiSettings();
  settingsManager.loadAppSettings();
  fingerManager.connect();

  if (!checkPairingValid()) {
    notifyClients("Security issue! Pairing with sensor is invalid. This could potentially be an attack! If the sensor is new or has been replaced by you do a (re)pairing in settings page.");
  }

  if (fingerManager.isFingerOnSensor() || !settingsManager.isWifiConfigured()) {
    Serial.println("Started WiFi-Config mode");
    fingerManager.setLedRingWifiConfig();

  } else {
    Serial.println("Started normal operating mode");
    currentMode = Mode::scan;

    if (initWifi()) {

      doRequest("POST", "POSTING from ESP32");

      if (fingerManager.connected) {
        fingerManager.setLedRingReady();
      } else {
        fingerManager.setLedRingError();
      }
    } else {
      fingerManager.setLedRingError();
      shouldReboot = true;
    }
  }

}

void loop() {
  // shouldReboot flag for supporting reboot through webui
  if (shouldReboot) {
    reboot();
  }

  // do the actual loop work
  switch (currentMode) {
  case Mode::scan:
    if (fingerManager.connected)
      doScan();
    break;

  case Mode::enroll:
    Serial.println("Mode Enroll");
    doEnroll();
    currentMode = Mode::scan; // switch back to scan mode after enrollment is done

    Serial.println("Enter in mode Scan");
    break;

  case Mode::maintenance:
    // do nothing, give webserver exclusive access to sensor (not thread-safe for concurrent calls)
    Serial.println("Mode Maintenance");
    break;
  }

  // enter maintenance mode (no continous scanning) if requested
  if (needMaintenanceMode) {
    currentMode = Mode::maintenance;
  }
}

