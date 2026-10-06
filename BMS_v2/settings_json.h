#pragma once
// Inställningar <-> JSON för webb-API:t. Bara ArduinoJson (testas på PC).
#include <ArduinoJson.h>
#include "settings.h"

void settingsToJson(const Settings& s, JsonObject o, bool maskPasswords);
// Uppdaterar s med de fält som finns i o. nullptr = OK, annars namnet på ogiltigt fält (s är då orörd).
const char* settingsFromJson(Settings& s, JsonObjectConst o);
