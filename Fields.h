/*
   ESP8266 + FastLED + IR Remote: https://github.com/jasoncoon/esp8266-fastled-webserver
   Copyright (C) 2016 Jason Coon

   This program is free software: you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation, either version 3 of the License, or
   (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

uint8_t power = 1;
uint8_t brightness = brightnessMap[brightnessIndex];

String getPower() {
  return String(power);
}

String getBrightness() {
  return String(brightness);
}

String getPattern() {
  return String(currentPatternIndex);
}

String getPatterns() {
  String json = "";

  for (uint8_t i = 0; i < patternCount; i++) {
    json += "\"" + patterns[i].name + "\"";
    if (i < patternCount - 1)
      json += ",";
  }

  return json;
}

String getPalette() {
  return String(currentPaletteIndex);
}

String getPalettes() {
  String json = "";

  for (uint8_t i = 0; i < paletteCount; i++) {
    json += "\"" + paletteNames[i] + "\"";
    if (i < paletteCount - 1)
      json += ",";
  }

  return json;
}

String getGlitter() {
  return String(glitter);
}

String getStrobe() {
  return String(strobe);
}

String getMicrophone() {
  return String(microphone);
}

String getAutoplay() {
  return String(autoplay);
}

String getAutoplayDuration() {
  return String(autoplayDuration);
}

String getAutocolor() {
  return String(autocolor);
}

String getAutocolorDuration() {
  return String(autocolorDuration);
}

String getSolidColor() {
  return String(solidColor.r) + "," + String(solidColor.g) + "," + String(solidColor.b);
}

String getSpeed() {
  return String(speed);
}

// UI Fields
FieldList fields = {
  { "power", "Power", BooleanFieldType, 0, 1, getPower },
  { "brightness", "Brightness", NumberFieldType, 1, 255, getBrightness },
  { "speed", "Speed", NumberFieldType, 1, 255, getSpeed },
  { "pattern", "Pattern", SectionFieldType },
  { "pattern", "Pattern", SelectFieldType, 0, patternCount, getPattern, getPatterns },
  { "autoplay", "Random", BooleanFieldType, 0, 1, getAutoplay },
  { "autoplayDuration", "Duration", NumberFieldType, 0, 255, getAutoplayDuration },
  { "palette", "Palette", SectionFieldType },
  { "palette", "Palette", SelectFieldType, 0, paletteCount, getPalette, getPalettes },
  { "autocolor", "Random", BooleanFieldType, 0, 1, getAutocolor },
  { "autocolorDuration", "Duration", NumberFieldType, 0, 255, getAutocolorDuration },
  { "glitter", "Glitter", SectionFieldType },
  { "glitter", "Glitter", BooleanFieldType, 0, 1, getGlitter },
  { "strobe", "Strobe", BooleanFieldType, 0, 1, getStrobe },
  { "microphone", "Microphone", BooleanFieldType, 0, 1, getMicrophone },  
  { "solidColor", "Solid color", SectionFieldType },
  { "solidColor", "Solid color", ColorFieldType, 0, 255, getSolidColor },
};

uint8_t fieldCount = ARRAY_SIZE(fields);
