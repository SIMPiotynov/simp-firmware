#ifndef SETTINGSMANAGER_H
#define SETTINGSMANAGER_H

#include <Preferences.h>
#include "global.h"

struct WifiSettings {
    String ssid = "Xiaomi mi 9T";
    String password = "cherchepas";
    String hostname = "esp32.develop";
};

struct AppSettings {
    String sensorPin = "00000000";
    String sensorPairingCode = "";
    bool   sensorPairingValid = false;
};

class SettingsManager {
  private:
    AppSettings appSettings;

    void saveAppSettings();

  public:
    bool loadAppSettings();

    AppSettings getAppSettings();
    void saveAppSettings(AppSettings newSettings);

    bool deleteAppSettings();

    String generateNewPairingCode();

};

#endif
