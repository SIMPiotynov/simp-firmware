#include <Arduino.h>
#include <iostream>
#include <sstream>
#include <string>

#include "player.h"

Melody getTrackPath(String type, const char track[]) {
    Melody melody;

    if (type.equals("file")) {
        String path = "/" + String(track) + ".rtttl";

        melody = MelodyFactory.loadRtttlFile(path);

        if (!melody) {
            Serial.println(path + " not found, try to load another one...");
        }
    } else {
       melody =  MelodyFactory.loadRtttlString(track);

        if (!melody) {
            Serial.println("Your custom ringtone dosen't work, loading entertainer ringtone...");
            melody = MelodyFactory.loadRtttlFile("/entertainer.rtttl");
        }
    }

    return melody;
}
