#ifndef SETTINGSMANAGER_H
#define SETTINGSMANAGER_H

#include <Preferences.h>
#include "global.h"

struct WifiSettings {
    String ssid = "Xiaomi mi 9T";
    String password = "cherchepas";
};

struct AppSettings {
    String sensorPin = "00000000";
    String sensorPairingCode = "";
    bool   sensorPairingValid = false;
};

class SettingsManager {
  private:
    AppSettings appSettings;
    WifiSettings wifiSettings;

    void saveAppSettings();
    void saveWifiSettings();

  public:
    bool loadAppSettings();
    bool loadWifiSettings();

    WifiSettings getWifiSettings();
    void saveWifiSettings(WifiSettings newSettings);

    AppSettings getAppSettings();
    void saveAppSettings(AppSettings newSettings);

    bool isWifiConfigured();

    bool deleteAppSettings();

    String generateNewPairingCode();

};

#endif
