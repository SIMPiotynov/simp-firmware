#include <Arduino.h>
#include <iostream>
#include <sstream>
#include <string>

#include "player.h"

Melody getTrackPath(String name) {
    String path = "/" + name + ".rtttl";

    Melody melody = MelodyFactory.loadRtttlFile(path);

    if (!melody) {
        Serial.println(path + " not found, try to load another one...");
    }

    return melody;
}
