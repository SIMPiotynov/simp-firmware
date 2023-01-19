#ifndef SETTINGSMANAGER_H
#define SETTINGSMANAGER_H

#include <Preferences.h>
#include "global.h"

struct WifiSettings {
    String ssid = "Freebox-32AEE6";
    String password = "s9cz5qch22q6hf59qrvz9b";
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
