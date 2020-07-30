/*
   ESP8266 FastLED WebServer: https://github.com/hyotynen/Dream-LED
   (C) Kimmo Hyötynen

   This program is free software: you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation, either version 3 of the License, or
   (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
   GNU General Public License for more details. 
   See <http://www.gnu.org/licenses/>.
*/

#define FASTLED_INTERRUPT_RETRY_COUNT 0
#include <FastLED.h>
FASTLED_USING_NAMESPACE

extern "C" {
#include "user_interface.h"
}

#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <ESP8266HTTPUpdateServer.h>
#include <FS.h>
#include <EEPROM.h>
#include "GradientPalettes.h"

#define ARRAY_SIZE(A) (sizeof(A) / sizeof((A)[0]))

#include "Field.h"

ESP8266WebServer webServer(80);
ESP8266HTTPUpdateServer httpUpdateServer;

#define DATA_PIN      2
#define LED_TYPE      WS2812B
#define COLOR_ORDER   GRB
#define NUM_LEDS      72

#define MILLI_AMPS         3000 // IMPORTANT: set the max milli-Amps of your power supply (4A = 4000mA). Separate PSU needed
#define FRAMES_PER_SECOND  500  // here you can control the speed. With the Access Point / Web Server the animations run a bit slower.

const bool apMode = false;

#include "Secrets.h" // this file is intentionally not included in the sketch, so nobody accidentally commits their secret information.
//char* ssid = "SSID";
//char* password = "PASSWD";

CRGB leds[NUM_LEDS];

const uint8_t brightnessCount = 5;
uint8_t brightnessMap[brightnessCount] = { 16, 32, 64, 128, 255 };
uint8_t brightnessIndex = 0;

uint8_t speed = 30;   // 1...255

CRGBPalette16 gCurrentPalette( CRGB::Black);
CRGBPalette16 gTargetPalette( CRGB::Black);

uint8_t currentPatternIndex = 0; // Index number of which pattern is current
uint8_t currentPaletteIndex = 0; // Index number of which pattern is current
uint8_t glitter = 0;
uint8_t strobe = 0;
bool strobePulse = false;
uint8_t microphone = 0;
uint8_t autoplay = 0;
uint8_t autocolor = 0;

uint8_t autoplayDuration = 10;
unsigned long autoPlayTimeout = 0;
uint8_t autocolorDuration = 10;
unsigned long autocolorTimeout = 0;

uint8_t gHue = 0; // rotating "base color" used by the patterns

CRGB solidColor = CRGB::Blue;

// scale the brightness of all pixels down
void dimAll(byte value)
{
  for (int i = 0; i < NUM_LEDS; i++) {
    leds[i].nscale8(value);
  }
}

// Sound handling and most of the sound reactive effects By: Andrew Tuline
#define MIC_PIN   A0                                          // Analog port for microphone
uint8_t squelch = 5;                                          // Anything below this is background noise, so we'll make it '0'.
int sample;                                                   // Current sample.
float sampleAvg = 0;                                          // Smoothed Average.
float micLev = 0;                                             // Used to convert returned value to have '0' as minimum.
uint8_t maxVol = 11;                                          // Reasonable value for constant volume for 'peak detector', as it won't always trigger.
bool samplePeak = 0;                                          // Boolean flag for peak. Responding routine must reset this flag.
int sampleAgc, multAgc;
uint8_t targetAgc = 60;                                       // This is our setPoint at 20% of max for the adjusted output.

typedef void (*Pattern)();
typedef Pattern PatternList[];
typedef struct {
  Pattern pattern;
  String name;
} PatternAndName;
typedef PatternAndName PatternAndNameList[];

// List of patterns to cycle through.  Each is defined as a separate function below.
PatternAndNameList patterns = {
  { soundBlocks,            "Blocks" },
  { bpm,                    "BPM" },
  { breathing,              "Breathing" },
  { rainbowSolid,           "Changing solid" },
  { colorWaves,             "Color waves" }, 
  { confetti,               "Confetti" },
  { fire,                   "Fire" },
  { rainbow,                "Gradient" },
  { heartBeat,              "Heart beat" },
  { juggle,                 "Juggle" },
  { rain,                   "Rain" },
  { runningLights,          "Running Lights" },
  { shootingStar,           "Shooting star" },
  { sinelon,                "Sinelon" },
  { discostrobe,            "Strobe" },
  { theaterChase,           "Theater" },
  { soundGradient,          "(Sound) Brightness" },
  { confettiSound,          "(Sound) Confetti" },
  { firewide,               "(Sound) Fire" },
  { jugglep,                "(Sound) Juggle" },
  { rainbowpeak,            "(Sound) Lightning" },
  { matrix,                 "(Sound) Matrix" },
  { fillnoise,              "(Sound) Noise" },
  { noisewide,              "(Sound) Noise 2" },
  { sinephase,              "(Sound) Phase" },
  { pixelblend,             "(Sound) Pixels" },
  { plasma,                 "(Sound) Plasma" },
  { plasma2,                "(Sound) Plasma 2" },
  { ripple,                 "(Sound) Ripple" },
  { onesine,                "(Sound) Sinewave" },
  { myvumeter,              "(Sound) VU Meter" },
  { showSolidColor,         "Solid Color" }    // This should be last
};

const uint8_t patternCount = ARRAY_SIZE(patterns);

typedef struct {
  CRGBPalette16 palette;
  String name;
} PaletteAndName;
typedef PaletteAndName PaletteAndNameList[];

const CRGBPalette16 palettes[] = {
  GMT_gray_gp,
  Blues_06_gp,
  ib_jul01_gp,
  christmas_candy_gp,
  CloudColors_p,
  GMT_drywet_gp,
  elevation_gp,
  es_emerald_dragon_08_gp,
  ForestColors_p,
  Fuschia_7_gp,
  gr65_hult_gp,
  HeatColors_p,  
  LavaColors_p,  
  OceanColors_p,
  PartyColors_p,
  bhw4_029_gp,
  es_pinksplash_08_gp,
  rgi_15_gp,
  RainbowColors_p,
  RainbowStripeColors_p,    
  ramp_gp,
  rainbowsherbet_gp,  
  subtle_gp,
  bhw1_07_gp,
  ib15_gp,
  Sunset_Real_gp,
  bhw1_26_gp,
  bhw1_06_gp,
  es_vintage_57_gp
};

const uint8_t paletteCount = ARRAY_SIZE(palettes);
const String paletteNames[paletteCount] = {
  "Black & white",
  "Blues",
  "Christmas",
  "Christmas candy",
  "Cloud",
  "Dry & wet",
  "Elevation",
  "Emerald",
  "Forest",
  "Fuschia",  
  "Grove",
  "Heat",
  "Lava",
  "Ocean",
  "Party",
  "Pastel",
  "Pink splash",
  "Purple",
  "Rainbow", 
  "Rainbow stripes",
  "Ramp",   
  "Sherbet",
  "Subtle",
  "Sun",  
  "Sunrise",
  "Sunset",
  "Tricolor",
  "Turquoise",
  "Vintage"
};

#include "Fields.h"

void setup() {
  WiFi.setSleepMode(WIFI_NONE_SLEEP);

  Serial.begin(115200);
  delay(100);
  Serial.setDebugOutput(true);

  FastLED.addLeds<LED_TYPE, DATA_PIN, COLOR_ORDER>(leds, NUM_LEDS);         // for WS2812 (Neopixel)
  FastLED.setDither(false);
  FastLED.setCorrection(TypicalLEDStrip);
  FastLED.setBrightness(brightness);
  FastLED.setMaxPowerInVoltsAndMilliamps(5, MILLI_AMPS);
  fill_solid(leds, NUM_LEDS, CRGB::Black);
  FastLED.show();

  EEPROM.begin(512);
  loadSettings();

  FastLED.setBrightness(brightness);

  Serial.println();
  Serial.print( F("Heap: ") ); Serial.println(system_get_free_heap_size());
  Serial.print( F("Boot Vers: ") ); Serial.println(system_get_boot_version());
  Serial.print( F("CPU: ") ); Serial.println(system_get_cpu_freq());
  Serial.print( F("SDK: ") ); Serial.println(system_get_sdk_version());
  Serial.print( F("Chip ID: ") ); Serial.println(system_get_chip_id());
  Serial.print( F("Flash ID: ") ); Serial.println(spi_flash_get_id());
  Serial.print( F("Flash Size: ") ); Serial.println(ESP.getFlashChipRealSize());
  Serial.print( F("Vcc: ") ); Serial.println(ESP.getVcc());
  Serial.println();

  SPIFFS.begin();
  {
    Serial.println("SPIFFS contents:");

    Dir dir = SPIFFS.openDir("/");
    while (dir.next()) {
      String fileName = dir.fileName();
      size_t fileSize = dir.fileSize();
      Serial.printf("FS File: %s, size: %s\n", fileName.c_str(), String(fileSize).c_str());
    }
    Serial.printf("\n");
  }

  if (apMode)
  {
    WiFi.mode(WIFI_AP);

    // Do a little work to get a unique-ish name. Append the
    // last two bytes of the MAC (HEX'd) to "Thing-":
    uint8_t mac[WL_MAC_ADDR_LENGTH];
    WiFi.softAPmacAddress(mac);
    String macID = String(mac[WL_MAC_ADDR_LENGTH - 2], HEX) +
                   String(mac[WL_MAC_ADDR_LENGTH - 1], HEX);
    macID.toUpperCase();
    String AP_NameString = "ESP8266 Thing " + macID;

    char AP_NameChar[AP_NameString.length() + 1];
    memset(AP_NameChar, 0, AP_NameString.length() + 1);

    for (int i = 0; i < AP_NameString.length(); i++)
      AP_NameChar[i] = AP_NameString.charAt(i);

    WiFi.softAP(AP_NameChar, WiFiAPPSK);

    Serial.printf("Connect to Wi-Fi access point: %s\n", AP_NameChar);
    Serial.println("and open http://192.168.4.1 in your browser");
  }
  else
  {
    WiFi.mode(WIFI_STA);
    Serial.printf("Connecting to %s\n", ssid);
    if (String(WiFi.SSID()) != String(ssid)) {
      WiFi.begin(ssid, password);
    }
  }

  httpUpdateServer.setup(&webServer);

  webServer.on("/all", HTTP_GET, []() {
    String json = getFieldsJson(fields, fieldCount);
    webServer.send(200, "text/json", json);
  });

  webServer.on("/fieldValue", HTTP_GET, []() {
    String name = webServer.arg("name");
    String value = getFieldValue(name, fields, fieldCount);
    webServer.send(200, "text/json", value);
  });

  webServer.on("/fieldValue", HTTP_POST, []() {
    String name = webServer.arg("name");
    String value = webServer.arg("value");
    String newValue = setFieldValue(name, value, fields, fieldCount);
    webServer.send(200, "text/json", newValue);
  });

  webServer.on("/power", HTTP_POST, []() {
    String value = webServer.arg("value");
    setPower(value.toInt());
    sendInt(power);
  });

  webServer.on("/speed", HTTP_POST, []() {
    String value = webServer.arg("value");
    speed = value.toInt();
    broadcastInt("speed", speed);
    sendInt(speed);
  });

  webServer.on("/solidColor", HTTP_POST, []() {
    String r = webServer.arg("r");
    String g = webServer.arg("g");
    String b = webServer.arg("b");
    setSolidColor(r.toInt(), g.toInt(), b.toInt());
    sendString(String(solidColor.r) + "," + String(solidColor.g) + "," + String(solidColor.b));
  });

  webServer.on("/pattern", HTTP_POST, []() {
    String value = webServer.arg("value");
    setPattern(value.toInt());
    sendInt(currentPatternIndex);
  });

  webServer.on("/patternName", HTTP_POST, []() {
    String value = webServer.arg("value");
    setPatternName(value);
    sendInt(currentPatternIndex);
  });

  webServer.on("/palette", HTTP_POST, []() {
    String value = webServer.arg("value");
    setPalette(value.toInt());
    sendInt(currentPaletteIndex);
  });

  webServer.on("/paletteName", HTTP_POST, []() {
    String value = webServer.arg("value");
    setPaletteName(value);
    sendInt(currentPaletteIndex);
  });

  webServer.on("/brightness", HTTP_POST, []() {
    String value = webServer.arg("value");
    setBrightness(value.toInt());
    sendInt(brightness);
  });

  webServer.on("/glitter", HTTP_POST, []() {
    String value = webServer.arg("value");
    setGlitter(value.toInt());
    sendInt(glitter);
  });

  webServer.on("/strobe", HTTP_POST, []() {
    String value = webServer.arg("value");
    setStrobe(value.toInt());
    sendInt(strobe);
  });

  webServer.on("/microphone", HTTP_POST, []() {
    String value = webServer.arg("value");
    setMicrophone(value.toInt());
    sendInt(microphone);
  });

  webServer.on("/autoplay", HTTP_POST, []() {
    String value = webServer.arg("value");
    setAutoplay(value.toInt());
    sendInt(autoplay);
  });

  webServer.on("/autoplayDuration", HTTP_POST, []() {
    String value = webServer.arg("value");
    setAutoplayDuration(value.toInt());
    sendInt(autoplayDuration);
  });

  webServer.on("/autocolor", HTTP_POST, []() {
    String value = webServer.arg("value");
    setAutocolor(value.toInt());
    sendInt(autocolor);
  });

  webServer.on("/autocolorDuration", HTTP_POST, []() {
    String value = webServer.arg("value");
    setAutocolorDuration(value.toInt());
    sendInt(autocolorDuration);
  });

  webServer.serveStatic("/", SPIFFS, "/", "max-age=86400");
  webServer.begin();
  Serial.println("HTTP web server started");

  autoPlayTimeout = millis() + (autoplayDuration * 1000);
}

void sendInt(uint8_t value)
{
  sendString(String(value));
}

void sendString(String value)
{
  webServer.send(200, "text/plain", value);
}

void broadcastInt(String name, uint8_t value)
{
  String json = "{\"name\":\"" + name + "\",\"value\":" + String(value) + "}";
  //  webSocketsServer.broadcastTXT(json);
}

void broadcastString(String name, String value)
{
  String json = "{\"name\":\"" + name + "\",\"value\":\"" + String(value) + "\"}";
}

void loop() {
  // Add entropy to random number generator; we use a lot of it.
  random16_add_entropy(random(65535));

  webServer.handleClient();

  if (power == 0) {
    fill_solid(leds, NUM_LEDS, CRGB::Black);
    FastLED.show();
    return;
  }

  static bool hasConnected = false;
  EVERY_N_SECONDS(1) {
    if (WiFi.status() != WL_CONNECTED) {
      //      Serial.printf("Connecting to %s\n", ssid);
      hasConnected = false;
    }
    else if (!hasConnected) {
      hasConnected = true;
      Serial.print("Connected! Open http://");
      Serial.print(WiFi.localIP());
      Serial.println(" in your browser");
    }
  }

  EVERY_N_MILLISECONDS(40) {
    // Blend the current palette to the next
    nblendPaletteTowardPalette( gCurrentPalette, gTargetPalette, 30); // 1...48, 1=slowest
    gHue++;  // slowly cycle the "base color" through the rainbow
  }

  if (autoplay && (millis() > autoPlayTimeout)) {
    if (microphone) setPattern(random(16, patternCount-1)); // pick random sound reactive pattern
    else setPattern(random(patternCount-1)); // pick random, but not solid color
    autoPlayTimeout = millis() + (autoplayDuration * 1000);
  }

  if (autocolor && (millis() > autocolorTimeout)) {
    setPalette(random(paletteCount)); // pick random
    autocolorTimeout = millis() + (autocolorDuration * 1000);
  }

  // Call the current pattern function once, updating the 'leds' array
  patterns[currentPatternIndex].pattern();

  if (glitter) {
      addGlitter(80);
  }

  EVERY_N_MILLISECONDS(100) {
    if (strobe) {
      if (strobePulse) {
        FastLED.setBrightness(brightness);
        strobePulse = false;
      }
      else {
        FastLED.setBrightness(0);
        strobePulse = true;
      }
    }
  }

  FastLED.show();

  // insert a delay to keep the framerate modest
  FastLED.delay(1000 / FRAMES_PER_SECOND);
}

void getSample() {
  int16_t micIn;                                              // Current sample starts with negative values and large values, which is why it's 16 bit signed.
  static long peakTime;

  if (microphone) micIn = analogRead(MIC_PIN);                // Poor man's analog Read.
  else micIn = beatsin8( 240, random(60), random(60)+50) + beatsin8( 480, random(70), random(70)+50) - beatsin8(1000,0,250) ;                       // Faking the audio signal used when microphone is turned off

  micLev = ((micLev * 31) + micIn) / 32;                      // Smooth it out over the last 32 samples for automatic centering.
  micIn -= micLev;                                            // Let's center it to 0 now.
  micIn = abs(micIn);                                         // And get the absolute value of each sample.
  sample = (micIn <= squelch) ? 0 : (sample + micIn) / 2;     // Using a ternary operator, the resultant sample is either 0 or it's a bit smoothed out with the last sample.
  sampleAvg = ((sampleAvg * 31) + sample) / 32;               // Smooth it out over the last 32 samples.

  if (sample > (sampleAvg+maxVol) && millis() > (peakTime + 50)) {    // Poor man's beat detection by seeing if sample > Average + some value.
    samplePeak = 1;                                                   // Then we got a peak, else we don't. Display routines need to reset the samplepeak value in case they miss the trigger.
    peakTime=millis();                
  }                                                           
}

void agcAvg() {                                                   // A simple averaging multiplier to automatically adjust sound sensitivity.
  multAgc = (sampleAvg < 1) ? targetAgc : targetAgc / sampleAvg;  // Make the multiplier so that sampleAvg * multiplier = setpoint
  sampleAgc = sample * multAgc;
  if (sampleAgc > 255) sampleAgc = 255;
}

void loadSettings()
{
  brightness = EEPROM.read(0);

  currentPatternIndex = EEPROM.read(1);
  if (currentPatternIndex < 0)
    currentPatternIndex = 0;
  else if (currentPatternIndex >= patternCount)
    currentPatternIndex = patternCount - 1;

  byte r = EEPROM.read(2);
  byte g = EEPROM.read(3);
  byte b = EEPROM.read(4);

  if (r == 0 && g == 0 && b == 0)
  {
  }
  else
  {
    solidColor = CRGB(r, g, b);
  }

  power = EEPROM.read(5);

  autoplay = EEPROM.read(6);
  autoplayDuration = EEPROM.read(7);

  currentPaletteIndex = EEPROM.read(8);
  if (currentPaletteIndex < 0)
    currentPaletteIndex = 0;
  else if (currentPaletteIndex >= paletteCount)
    currentPaletteIndex = paletteCount - 1;
  gCurrentPalette = palettes[currentPaletteIndex];
  gTargetPalette = palettes[currentPaletteIndex];
  glitter = EEPROM.read(9);
  autocolor = EEPROM.read(10);
  autocolorDuration = EEPROM.read(11);
  strobe = EEPROM.read(12);
  microphone = EEPROM.read(13);
}

void setPower(uint8_t value)
{
  power = value == 0 ? 0 : 1;

  EEPROM.write(5, power);
  EEPROM.commit();

  broadcastInt("power", power);
}

void setGlitter(uint8_t value)
{
  glitter = value == 0 ? 0 : 1;

  EEPROM.write(9, glitter);
  EEPROM.commit();

  broadcastInt("glitter", glitter);
}

void setStrobe(uint8_t value)
{
  strobe = value == 0 ? 0 : 1;
  if (strobe == 0) FastLED.setBrightness(brightness);

  EEPROM.write(12, strobe);
  EEPROM.commit();

  broadcastInt("strobe", strobe);
}

void setMicrophone(uint8_t value)
{
  microphone = value == 0 ? 0 : 1;

  EEPROM.write(13, microphone);
  EEPROM.commit();

  broadcastInt("microphone", microphone);
}

void setAutoplay(uint8_t value)
{
  autoplay = value == 0 ? 0 : 1;

  EEPROM.write(6, autoplay);
  EEPROM.commit();

  broadcastInt("autoplay", autoplay);
}

void setAutoplayDuration(uint8_t value)
{
  autoplayDuration = value;

  EEPROM.write(7, autoplayDuration);
  EEPROM.commit();

  autoPlayTimeout = millis() + (autoplayDuration * 1000);

  broadcastInt("autoplayDuration", autoplayDuration);
}

void setAutocolor(uint8_t value)
{
  autocolor = value == 0 ? 0 : 1;

  EEPROM.write(10, autocolor);
  EEPROM.commit();

  broadcastInt("autocolor", autocolor);
}

void setAutocolorDuration(uint8_t value)
{
  autocolorDuration = value;

  EEPROM.write(11, autocolorDuration);
  EEPROM.commit();

  autocolorTimeout = millis() + (autocolorDuration * 1000);

  broadcastInt("autocolorDuration", autocolorDuration);
}

void setSolidColor(CRGB color)
{
  setSolidColor(color.r, color.g, color.b);
}

void setSolidColor(uint8_t r, uint8_t g, uint8_t b)
{
  solidColor = CRGB(r, g, b);

  EEPROM.write(2, r);
  EEPROM.write(3, g);
  EEPROM.write(4, b);
  EEPROM.commit();

  setPattern(patternCount - 1);  // Must match the place in the list of patterns
  broadcastString("color", String(solidColor.r) + "," + String(solidColor.g) + "," + String(solidColor.b));
}

// increase or decrease the current pattern number, and wrap around at the ends
void adjustPattern(bool up)
{
  if (up)
    currentPatternIndex++;
  else
    currentPatternIndex--;

  // wrap around at the ends
  if (currentPatternIndex < 0)
    currentPatternIndex = patternCount - 1;
  if (currentPatternIndex >= patternCount)
    currentPatternIndex = 0;

  if (autoplay == 0) {
    EEPROM.write(1, currentPatternIndex);
    EEPROM.commit();
  }

  broadcastInt("pattern", currentPatternIndex);
}

void setPattern(uint8_t value)
{
  if (value >= patternCount)
    value = patternCount - 1;

  currentPatternIndex = value;
  FastLED.setBrightness(brightness);  // Reset brightness back if some pattern has adjusted it
  
  if (autoplay == 0) {
    EEPROM.write(1, currentPatternIndex);
    EEPROM.commit();
  }

  broadcastInt("pattern", currentPatternIndex);
}

void setPatternName(String name)
{
  for (uint8_t i = 0; i < patternCount; i++) {
    if (patterns[i].name == name) {
      setPattern(i);
      break;
    }
  }
}

void setPalette(uint8_t value)
{
  if (value >= paletteCount)
    value = paletteCount - 1;

  currentPaletteIndex = value;
  gTargetPalette = palettes[currentPaletteIndex];
  if (autocolor == 0) {
     EEPROM.write(8, currentPaletteIndex);
     EEPROM.commit();
  }   

  broadcastInt("palette", currentPaletteIndex);
}

void setPaletteName(String name)
{
  for (uint8_t i = 0; i < paletteCount; i++) {
    if (paletteNames[i] == name) {
      setPalette(i);
      break;
    }
  }
}

void adjustBrightness(bool up)
{
  if (up && brightnessIndex < brightnessCount - 1)
    brightnessIndex++;
  else if (!up && brightnessIndex > 0)
    brightnessIndex--;

  brightness = brightnessMap[brightnessIndex];

  FastLED.setBrightness(brightness);

  EEPROM.write(0, brightness);
  EEPROM.commit();

  broadcastInt("brightness", brightness);
}

void setBrightness(uint8_t value)
{
  if (value > 255)
    value = 255;
  else if (value < 0) value = 0;

  brightness = value;

  FastLED.setBrightness(brightness);

  EEPROM.write(0, brightness);
  EEPROM.commit();

  broadcastInt("brightness", brightness);
}

void addGlitter( uint8_t chanceOfGlitter)
{
  if ( random8() < chanceOfGlitter) {
    leds[ random16(NUM_LEDS) ] += CRGB::White;
  }
}

void waveit() {                                                                 // Shifting pixels from the center to the left and right.
  for (int i = NUM_LEDS - 1; i > NUM_LEDS/2; i--) {                             // Move to the right.
    leds[i] = leds[i - 1];
  }

  for (int i = 0; i < NUM_LEDS/2; i++) {                                        // Move to the left.
    leds[i] = leds[i + 1];
  }
}


//
// PATTERNS
//

void showSolidColor()
{
  fill_solid(leds, NUM_LEDS, solidColor);
}

// Patterns modified from FastLED example DemoReel100: https://github.com/FastLED/FastLED/blob/master/examples/DemoReel100/DemoReel100.ino

void rainbow()
{
  gHue = (int) gHue + speed / 20;
  fill_palette( leds, NUM_LEDS, gHue, 6, gCurrentPalette, 255, LINEARBLEND);
}

void rainbowSolid()
{
  gHue = (int) gHue + speed / 20;
  CRGB color = ColorFromPalette(gCurrentPalette, gHue, 255);
  fill_solid(leds, NUM_LEDS, color);
}

void confetti()
{
  gHue = (int) gHue + speed / 20;
  fadeToBlackBy( leds, NUM_LEDS, speed / 4);
  int pos = random16(NUM_LEDS);
  leds[pos] += ColorFromPalette(gCurrentPalette, gHue + random8(64));
}

void sinelon()
{
  // a colored dot sweeping back and forth, with fading trails
  fadeToBlackBy( leds, NUM_LEDS, 20);
  int pos = beatsin16(speed, 0, NUM_LEDS);
  static int prevpos = 0;
  CRGB color = ColorFromPalette(gCurrentPalette, gHue, 255);
  if ( pos < prevpos ) {
    fill_solid( leds + pos, (prevpos - pos) + 1, color);
  } else {
    fill_solid( leds + prevpos, (pos - prevpos) + 1, color);
  }
  prevpos = pos;
}

void bpm()
{
  // colored stripes pulsing at a defined Beats-Per-Minute (BPM)
  uint8_t beat = beatsin8( speed, 64, 255);
  CRGBPalette16 palette = gCurrentPalette;
  for ( int i = 0; i < NUM_LEDS; i++) {
    leds[i] = ColorFromPalette(palette, gHue + (i * 2), beat - gHue + (i * 10));
  }
}

void juggle()
{
  static uint8_t    numdots =   4; // Number of dots in use.
  static uint8_t   faderate =   2; // How long should the trails be. Very low value = longer trails.
  static uint8_t   basebeat = (int) speed / 2.7 + 0.5; // Higher = faster movement.

  static uint8_t lastSecond =  99;  // Static variable, means it's only defined once. This is our 'debounce' variable.
  uint8_t secondHand = (millis() / 1000) % 30; // IMPORTANT!!! Change '30' to a different value to change duration of the loop.

  if (lastSecond != secondHand) { // Debounce to make sure we're not repeating an assignment.
    lastSecond = secondHand;
    switch (secondHand) {
      case  0: numdots = 1; basebeat = (int) speed / 3 + 2; faderate = 2; break; // You can change values here, one at a time , or altogether.
      case 10: numdots = 4; basebeat = (int) speed / 2.6 + 3; faderate = 8; break;
      case 20: numdots = 8; basebeat = (int) speed / 3.2 + 4; faderate = 8; break; // Only gets called once, and not continuously for the next several seconds. Therefore, no rainbows.
      case 30: break;
    }
  }

  // Several colored dots, weaving in and out of sync with each other
  fadeToBlackBy(leds, NUM_LEDS, faderate);
  for ( int i = 0; i < numdots; i++) {
    //beat16 is a FastLED 3.1 function
    leds[beatsin16(basebeat + i + numdots, 0, NUM_LEDS)] += ColorFromPalette(gCurrentPalette, gHue, 255);
  }
}

// based on FastLED example Fire2012WithPalette: https://github.com/FastLED/FastLED/blob/master/examples/Fire2012WithPalette/Fire2012WithPalette.ino
void fire()
{
  CRGBPalette16 palette = gCurrentPalette;
  uint8_t cooling = 50;
  uint8_t sparking = speed;
  bool up = true;

  fill_solid(leds, NUM_LEDS, CRGB::Black);

  // Add entropy to random number generator; we use a lot of it.
  random16_add_entropy(random(256));

  // Array of temperature readings at each simulation cell
  static byte heat[NUM_LEDS];

  byte colorindex;

  // Step 1.  Cool down every cell a little
  for ( uint16_t i = 0; i < NUM_LEDS; i++) {
    heat[i] = qsub8( heat[i],  random8(0, ((cooling * 10) / NUM_LEDS) + 2));
  }

  // Step 2.  Heat from each cell drifts 'up' and diffuses a little
  for ( uint16_t k = NUM_LEDS - 1; k >= 2; k--) {
    heat[k] = (heat[k - 1] + heat[k - 2] + heat[k - 2] ) / 3;
  }

  // Step 3.  Randomly ignite new 'sparks' of heat near the bottom
  if ( random8() < sparking ) {
    int y = random8(7);
    heat[y] = qadd8( heat[y], random8(160, 255) );
  }

  // Step 4.  Map from heat cells to LED colors
  for ( uint16_t j = 0; j < NUM_LEDS; j++) {
    // Scale the heat value from 0-255 down to 0-240
    // for best results with color palettes.
    colorindex = scale8(heat[j], 190);

    CRGB color = ColorFromPalette(palette, colorindex);

    if (up) {
      leds[j] = color;
    }
    else {
      leds[(NUM_LEDS - 1) - j] = color;
    }
  }
}

// ColorWavesWithPalettes by Mark Kriegsman: https://gist.github.com/kriegsman/8281905786e8b2632aeb
// This function draws color waves with an ever-changing,
// widely-varying set of parameters, using a color palette.
void colorWaves()
{
  static uint16_t sPseudotime = 0;
  static uint16_t sLastMillis = 0;
  static uint16_t sHue16 = 0;

  uint8_t brightdepth = beatsin88( 341, 96, 224);
  uint16_t brightnessthetainc16 = beatsin88( speed*64, (25 * 256), (40 * 256));
  uint8_t msmultiplier = beatsin88(147, 23, 60);

  uint16_t hue16 = sHue16;//gHue * 256;
  uint16_t hueinc16 = beatsin88(speed*32, 300, 1500);

  uint16_t ms = millis();
  uint16_t deltams = ms - sLastMillis ;
  sLastMillis  = ms;
  sPseudotime += deltams * msmultiplier;
  sHue16 += deltams * beatsin88( 400, 5, 9);
  uint16_t brightnesstheta16 = sPseudotime;

  for ( uint16_t i = 0 ; i < NUM_LEDS; i++) {
    hue16 += hueinc16;
    uint8_t hue8 = hue16 / 256;
    uint16_t h16_128 = hue16 >> 7;
    if ( h16_128 & 0x100) {
      hue8 = 255 - (h16_128 >> 1);
    } else {
      hue8 = h16_128 >> 1;
    }

    brightnesstheta16  += brightnessthetainc16;
    uint16_t b16 = sin16( brightnesstheta16  ) + 32768;

    uint16_t bri16 = (uint32_t)((uint32_t)b16 * (uint32_t)b16) / 65536;
    uint8_t bri8 = (uint32_t)(((uint32_t)bri16) * brightdepth) / 65536;
    bri8 += (255 - brightdepth);

    uint8_t index = hue8;
    index = scale8( index, 240);

    CRGB newcolor = ColorFromPalette( gCurrentPalette, index, bri8);

    uint16_t pixelnumber = i;
    pixelnumber = (NUM_LEDS - 1) - pixelnumber;

    nblend( leds[pixelnumber], newcolor, 128);
  }
}

void discostrobe()
{
  // First, we black out all the LEDs
  fill_solid( leds, NUM_LEDS, CRGB::Black);

  delayToSyncFrameRate( speed);
  // To achive the strobe effect, we actually only draw lit pixels
  // every Nth frame (e.g. every 4th frame).  
  // sStrobePhase is a counter that runs from zero to kStrobeCycleLength-1,
  // and then resets to zero.  
  const uint8_t kStrobeCycleLength = 4; // light every Nth frame
  static uint8_t sStrobePhase = 0;
  sStrobePhase = sStrobePhase + 1;
  if( sStrobePhase >= kStrobeCycleLength ) { 
    sStrobePhase = 0; 
  }

  // We only draw lit pixels when we're in strobe phase zero; 
  // in all the other phases we leave the LEDs all black.
  if( sStrobePhase == 0 ) {

    // The dash spacing cycles from 4 to 9 and back, 8x/min (about every 7.5 sec)
    uint8_t dashperiod= beatsin8( 8/*cycles per minute*/, 4,10);
    // The width of the dashes is a fraction of the dashperiod, with a minimum of one pixel
    uint8_t dashwidth = (dashperiod / 4) + 1;
    
    // The distance that the dashes move each cycles varies 
    // between 1 pixel/cycle and half-the-dashperiod/cycle.
    // At the maximum speed, it's impossible to visually distinguish
    // whether the dashes are moving left or right, and the code takes
    // advantage of that moment to reverse the direction of the dashes.
    // So it looks like they're speeding up faster and faster to the
    // right, and then they start slowing down, but as they do it becomes
    // visible that they're no longer moving right; they've been 
    // moving left.  Easier to see than t o explain.
    //
    // The dashes zoom back and forth at a speed that 'goes well' with
    // most dance music, a little faster than 120 Beats Per Minute.  You
    // can adjust this for faster or slower 'zooming' back and forth.
    uint8_t zoomBPM = speed;
    int8_t  dashmotionspeed = beatsin8( (zoomBPM /2), 1,dashperiod);
    // This is where we reverse the direction under cover of high speed
    // visual aliasing.
    if( dashmotionspeed >= (dashperiod/2)) { 
      dashmotionspeed = 0 - (dashperiod - dashmotionspeed );
    }
    
    // The hueShift controls how much the hue of each dash varies from 
    // the adjacent dash.  If hueShift is zero, all the dashes are the 
    // same color. If hueShift is 128, alterating dashes will be two
    // different colors.  And if hueShift is range of 10..40, the
    // dashes will make rainbows.
    // Initially, I just had hueShift cycle from 0..130 using beatsin8.
    // It looked great with very low values, and with high values, but
    // a bit 'busy' in the middle, which I didnt like.
    //   uint8_t hueShift = beatsin8(2,0,130);
    //
    // So instead I layered in a bunch of 'cubic easings'
    // (see http://easings.net/#easeInOutCubic )
    // so that the resultant wave cycle spends a great deal of time
    // "at the bottom" (solid color dashes), and at the top ("two
    // color stripes"), and makes quick transitions between them.
    uint8_t cycle = beat8(2); // two cycles per minute
    uint8_t easedcycle = ease8InOutCubic( ease8InOutCubic( cycle));
    uint8_t wavecycle = cubicwave8( easedcycle);
    uint8_t hueShift = scale8( wavecycle,130);


    // Each frame of the animation can be repeated multiple times.
    // This slows down the apparent motion, and gives a more static
    // strobe effect.  After experimentation, I set the default to 1.
    uint8_t strobesPerPosition = 1; // try 1..4


    // Now that all the parameters for this frame are calculated,
    // we call the 'worker' function that does the next part of the work.
    discoWorker( dashperiod, dashwidth, dashmotionspeed, strobesPerPosition, hueShift);
  }  
}


// discoWorker updates the positions of the dashes, and calls the draw function
//
void discoWorker( 
    uint8_t dashperiod, uint8_t dashwidth, int8_t  dashmotionspeed,
    uint8_t stroberepeats,
    uint8_t huedelta)
 {
  static uint8_t sRepeatCounter = 0;
  static int8_t sStartPosition = 0;
  static uint8_t sStartHue = 0;

  // Always keep the hue shifting a little
  sStartHue += 1;

  // Increment the strobe repeat counter, and
  // move the dash starting position when needed.
  sRepeatCounter = sRepeatCounter + 1;
  if( sRepeatCounter>= stroberepeats) {
    sRepeatCounter = 0;
    
    sStartPosition = sStartPosition + dashmotionspeed;
    
    // These adjustments take care of making sure that the
    // starting hue is adjusted to keep the apparent color of 
    // each dash the same, even when the state position wraps around.
    if( sStartPosition >= dashperiod ) {
      while( sStartPosition >= dashperiod) { sStartPosition -= dashperiod; }
      sStartHue  -= huedelta;
    } else if( sStartPosition < 0) {
      while( sStartPosition < 0) { sStartPosition += dashperiod; }
      sStartHue  += huedelta;
    }
  }

  // draw dashes with full brightness (value), and somewhat
  // desaturated (whitened) so that the LEDs actually throw more light.
  const uint8_t kSaturation = 208;
  const uint8_t kValue = 255;

  // call the function that actually just draws the dashes now
  drawRainbowDashes( sStartPosition, NUM_LEDS-1, 
                     dashperiod, dashwidth, 
                     sStartHue, huedelta, 
                     kSaturation, kValue);
}


// drawRainbowDashes - draw rainbow-colored 'dashes' of light along the led strip:
//   starting from 'startpos', up to and including 'lastpos'
//   with a given 'period' and 'width'
//   starting from a given hue, which changes for each successive dash by a 'huedelta'
//   at a given saturation and value.
//
//   period = 5, width = 2 would be  _ _ _ X X _ _ _ Y Y _ _ _ Z Z _ _ _ A A _ _ _ 
//                                   \-------/       \-/
//                                   period 5      width 2
//
static void drawRainbowDashes( 
  uint8_t startpos, uint16_t lastpos, uint8_t period, uint8_t width, 
  uint8_t huestart, uint8_t huedelta, uint8_t saturation, uint8_t value)
{
  uint8_t hue = huestart;
  for( uint16_t i = startpos; i <= lastpos; i += period) {
    // Switched from HSV color wheel to color palette
    // Was: CRGB color = CHSV( hue, saturation, value); 
    CRGB color = ColorFromPalette( gCurrentPalette, hue, value, NOBLEND);
    
    // draw one dash
    uint16_t pos = i;
    for( uint8_t w = 0; w < width; w++) {
      leds[ pos ] = color;
      pos++;
      if( pos >= NUM_LEDS) {
        break;
      }
    }
    
    hue += huedelta;
  }
}

// delayToSyncFrameRate - delay how many milliseconds are needed
//   to maintain a stable frame rate.
static void delayToSyncFrameRate( uint8_t framesPerSecond)
{
  static uint32_t msprev = 0;
  uint32_t mscur = millis();
  uint16_t msdelta = mscur - msprev;
  uint16_t mstargetdelta = 1000 / framesPerSecond;
  if( msdelta < mstargetdelta) {
    delay( mstargetdelta - msdelta);
  }
  msprev = mscur;
}

void theaterChase() {
    static uint16_t delta = 0;
    delay(300-speed);
    
    fadeToBlackBy(leds, NUM_LEDS, 200);

    for (uint16_t i = 0; i+delta < NUM_LEDS; i=i+3) {
        leds[(i+delta)] = ColorFromPalette(gCurrentPalette, gHue, 255);
    }

    delta = (delta + 1) % 3;
}

void runningLights() {
    const uint8_t num_waves = 3; // results in three full sine waves across LED strip
    static uint16_t delta = 0;
    delay(102-speed/2.5);

    for (uint16_t i = 0; i < NUM_LEDS; i++) {
        uint16_t a = num_waves*(i+delta)*255/(NUM_LEDS-1);
        // this pattern normally runs from right-to-left, so flip it by using negative indexing
        uint16_t ni = (NUM_LEDS-1) - i;
        leds[ni] = ColorFromPalette(gCurrentPalette, gHue, sin8(a));
    }

    delta = (delta + 1) % (NUM_LEDS/num_waves);
}

//star_size – the number of LEDs that represent the star, not counting the tail of the star.
//star_trail_decay - how fast the star trail decays. A larger number makes the tail short and/or disappear faster.
//spm - stars per minute
void shootingStar() {
    int8_t star_size = 5;
    uint8_t star_trail_decay = 40;
        
    static uint16_t start_pos = random16((NUM_LEDS/4)*3, NUM_LEDS);
    static uint16_t stop_pos = random16(0, NUM_LEDS/2);

    const uint16_t cool_down_interval = 60000/speed;
    static uint32_t cdi_pm = 0; // cool_down_interval_previous_millis
    static uint16_t pos = start_pos;

    fade_randomly(128, star_trail_decay);

    if ( (millis() - cdi_pm) > cool_down_interval ) {
        for (uint8_t i = 0; i < star_size; i++) {
            leds[pos-i] += ColorFromPalette(gCurrentPalette, gHue, 255);
        }
        pos--;
        
        if (pos-1 < stop_pos+1) {
            start_pos = random16((NUM_LEDS/4)*3, NUM_LEDS);
            stop_pos = random16(0, NUM_LEDS/2);
            pos = start_pos;
            cdi_pm = millis();
        }
    }
}

void fade_randomly(uint8_t chance_of_fade, uint8_t decay) {
    for (uint16_t i = 0; i < NUM_LEDS; i++) {
        if (chance_of_fade > random8()) {
            leds[i].fadeToBlackBy(decay);
        }
    }
}

void heartBeat() {
    const uint16_t start_pos = NUM_LEDS/2;
    static uint16_t pos = start_pos;
    uint8_t cell_size = 5;
    const uint16_t cool_down_interval = abs(speed-255)*10;
    static uint32_t cdi_pm = 0; // cool_down_interval_previous_millis
    
    fadeToBlackBy(leds, NUM_LEDS, 30);

    if ( (millis() - cdi_pm) > cool_down_interval ) {
        for (uint8_t i = 0; i < cell_size; i++) {
            uint16_t pi = pos+(cell_size-1)-i;
            uint16_t ni = (NUM_LEDS-1) - pi;
            leds[pi] = ColorFromPalette(gCurrentPalette, gHue, 255);
            leds[ni] = ColorFromPalette(gCurrentPalette, gHue, 255);
        }
        pos++;
        if (pos+(cell_size-1) >= NUM_LEDS) {
            cdi_pm = millis();
            pos = start_pos;
        }
    }        
}

void soundBlocks() {
    uint8_t hue = random8();
    uint16_t block_start = random16(NUM_LEDS);
    uint8_t block_size = random8(3,8);
    
    for (uint8_t i = 0; i < block_size; i++) {
        uint16_t pos = (NUM_LEDS+block_start+i) % NUM_LEDS;
        leds[pos] =  ColorFromPalette(gCurrentPalette, hue, 255);
    }
    delay (abs(speed-255));
}

void rain() {
    const uint8_t dropCount = 7;
    static uint8_t dropDelay[dropCount];
    static uint8_t dropPos[dropCount];
    static uint32_t dropPM[dropCount];
    static uint32_t dropHue[dropCount];
    static bool init = true;
    static uint32_t cdi_pm = 0;
   
    //fadeToBlackBy(leds, NUM_LEDS, 1);

    if (init) {  // Initialize
        for (uint8_t n = 0; n < dropCount; n++) {
            dropDelay[n] = random16(abs(speed-256)/5,abs(speed-256)/3+5);
            dropPos[n] = random8(0,NUM_LEDS);
            dropPM[n] = 0;
            dropHue[n] = random8();
        }
        init = false;    
    }

    if ( (millis() - cdi_pm) > 15 ) {
        fadeToBlackBy(leds, NUM_LEDS, 1);
        cdi_pm = millis();
    }
    
    for (uint8_t n = 0; n < dropCount; n++) {  // Advance
        if ( (millis() - dropPM[n]) > dropDelay[n] ) {
            for (uint8_t i = 0; i < 2; i++) {
                if (i == 0) leds[dropPos[n]-i] = ColorFromPalette(gCurrentPalette, gHue+dropHue[n], random16(10,80));
                else leds[dropPos[n]-i] = blend(ColorFromPalette(gCurrentPalette, gHue+dropHue[n], 255), CRGB::White, 32);  // blend the drop slightly towards white
            }
            dropPos[n]--;
        
            if (dropPos[n]-1 < 0) {  // End reached
                leds[0] = ColorFromPalette(gCurrentPalette, gHue+dropHue[n], random16(10,80));
                dropPos[n] = NUM_LEDS;
                dropDelay[n] = random16(abs(speed-256)/4,abs(speed-256)/3+1);
                dropHue[n] = random8();
            }
            dropPM[n] = millis();
        }
    }
}

void breathing() {
    static uint8_t delta = 0;
    uint8_t b = triwave8(delta);

    b = map(b, 0, 255, 0, brightness);

    CRGB color = ColorFromPalette(gCurrentPalette, gHue, 255);
    fill_solid(leds, NUM_LEDS, color);
    FastLED.setBrightness(b);

    delta+=(int)(speed/5)+1;
}

void confettiSound() {
    getSample();                                                // Sample the microphone.
    agcAvg();                                                   // Calculate the adjusted value as sampleAvg.
    fadeToBlackBy(leds, NUM_LEDS, 1);                     // 8 bit, 1 = slow, 255 = fast
    if (sampleAgc > 0) {
       leds[(millis() % (NUM_LEDS))] = ColorFromPalette(gCurrentPalette, sampleAgc, sampleAgc);
    }   
}

void fillnoise() {                                                             // Another perlin noise based routine.
// Local definitions
  #define xscale 160
  #define yscale 160

// Persistent local variables
  static int16_t xdist;                                                         // A random number for our noise generator.
  static int16_t ydist;

  getSample();                                                                // Sample sound, measure averages and detect peak.
  agcAvg();

  if (sampleAvg > NUM_LEDS) sampleAvg = NUM_LEDS;                               // Clip the sampleAvg to maximize at NUM_LEDS.
  
  for (int i= (NUM_LEDS-sampleAvg/2)/2; i<(NUM_LEDS+sampleAvg/2)/2; i++) {      // The louder the sound, the wider the soundbar.
    uint8_t index = inoise8(i*sampleAvg+xdist, ydist+i*sampleAvg);              // Get a value from the noise function. I'm using both x and y axis.

    leds[i] = ColorFromPalette(gCurrentPalette, index, sampleAgc, LINEARBLEND);   // With that value, look up the 8 bit colour palette value and assign it to the current LED.
  }                                                                             // Effect is a NOISE bar the width of sampleAvg. Very fun. By Andrew Tuline.

  xdist += beatsin8(5,0,3);                                                     // Moving forward in the NOISE field, but with a sine motion.
  ydist += beatsin8(4,0,3);                                                     // Moving sideways in the NOISE field, but with a sine motion.

  waveit();                                                                     // Move the pixels to the left/right, but not too fast.
  
  fadeToBlackBy(leds+NUM_LEDS/2-1, 2, 64);                                     // Fade the center, while waveit moves everything out to the edges.
}

void firewide() {                                                               // Create fire based on noise and sampleAvg. 
// Local definitions
  #define xscale 20                                                             // How far apart they are
  #define yscale 3                                                              // How fast they move

// Temporary local variables
  uint16_t index = 0;                                                            // Current colour lookup value.

  getSample();                                                                // Sample sound, measure averages and detect peak.
  agcAvg();

  for(int i = 0; i < NUM_LEDS; i++) {

    index = inoise8(i*xscale,millis()*yscale*NUM_LEDS/255);                       // X location is constant, but we move along the Y at the rate of millis(). By Andrew Tuline.

    index = (255 - i*256/NUM_LEDS) * index / 128;                                 // Now we need to scale index so that it gets blacker as we get close to one of the ends
                                                                                
    leds[NUM_LEDS/2-i/2+1] = ColorFromPalette(gCurrentPalette, index, sampleAvg*4, NOBLEND);      // With that value, look up the 8 bit colour palette value and assign it to the current LED. 
    leds[NUM_LEDS/2+i/2-1] = ColorFromPalette(gCurrentPalette, index, sampleAvg*4, NOBLEND);      // With that value, look up the 8 bit colour palette value and assign it to the current LED.    
  }                                                                               // The higher the value of i => the higher up the palette index (see palette definition).
}

void jugglep() {                                                                // Use the juggle routine, but adjust the timebase based on sampleAvg for some randomness.
// Persistent local variables
  static int thistime = 20;                                                     // Time shifted value keeps changing thus interrupting the juggle pattern.

// Temporary local variables

  getSample();                                                                // Sample sound, measure averages and detect peak.
  agcAvg();

  fadeToBlackBy(leds, NUM_LEDS, 32);                                            // Fade the strand.
  
  leds[beatsin16(thistime,0,NUM_LEDS-1, 0, 0)] += ColorFromPalette( gCurrentPalette, millis()/4, sampleAgc);
  leds[beatsin16(thistime-3,0,NUM_LEDS-1, 0, 0)] += ColorFromPalette( gCurrentPalette, millis()/4, sampleAgc);
  
  EVERY_N_MILLISECONDS(250) {
    thistime = sampleAvg/2;                                                     // Change the beat frequency every 250 ms. By Andrew Tuline.
  } 
}

void matrix() {                                                  // A 'Matrix' like display using sampleavg for brightness.
// Local definitions
  int8_t     thisdir = -1;                                           // Standard direction is either -1 or 1. Used as a multiplier rather than boolean and is initialized by EEPROM.

  getSample();                                                                // Sample sound, measure averages and detect peak.
  agcAvg();

  if (thisdir == 1) {
      leds[0] = ColorFromPalette(gCurrentPalette, millis(), sampleAgc*2); 
  } else {
      leds[NUM_LEDS-1] = ColorFromPalette( gCurrentPalette, millis(), sampleAgc*2);
  }

  if (thisdir == 1) {
    for (int i = NUM_LEDS-1; i >0 ; i-- ) leds[i] = leds[i-1];
  } else {
    for (int i = 0; i < NUM_LEDS-1 ; i++ ) leds[i] = leds[i+1];
  }
}

void myvumeter() {                                                        // A vu meter. Grabbed the falling LED from Reko MeriÃ¶.
// Local definitions
  #define GRAVITY 3

// Persistent local variables
  static uint8_t topLED;
  static int gravityCounter = 0;

// Temporary local variables
  uint8_t tempsamp = constrain(sampleAvg*2,0,NUM_LEDS-1);                     // Keep the sample from overflowing.

  getSample();                                                            // Sample sound, measure averages and detect peak.
  agcAvg();

  fadeToBlackBy(leds, NUM_LEDS, 160);
    
  for (int i=0; i<tempsamp; i++) {
    uint8_t index = inoise8(i*sampleAvg+millis(), 5000+i*sampleAvg); 
    leds[i] = ColorFromPalette(gCurrentPalette, index, 200);
  }

  if (tempsamp >= topLED)
    topLED = tempsamp;
  else if (gravityCounter % GRAVITY == 0)
    topLED--;

  if (topLED > 0) {
    leds[topLED] = ColorFromPalette(gCurrentPalette, millis(), 255);   // LED falls when the volume goes down.
  }
  
  gravityCounter = (gravityCounter + 1) % GRAVITY;  
}

void noisewide() {
// Local definitions
  #define GRAVITY 5

// Persistent local variables
  static uint8_t topLED;
  static int gravityCounter = 0;

// Temporary local variables
  uint8_t tempsamp = constrain(sampleAvg,0,NUM_LEDS/2-1);                       // Keep the sample from overflowing.

  getSample();                                                                // Sample sound, measure averages and detect peak.
  agcAvg();

  fadeToBlackBy(leds, NUM_LEDS, 160);
    
  for (int i=0; i<tempsamp; i++) {
    uint8_t index = inoise8(i*sampleAvg+millis(), 5000+i*sampleAvg*2); 
    leds[NUM_LEDS/2-i] = ColorFromPalette(gCurrentPalette, index, sampleAvg*2);
    leds[NUM_LEDS/2+i] = ColorFromPalette(gCurrentPalette, index, sampleAvg*2);
  }

  if (tempsamp >= topLED)
    topLED = tempsamp;
  else if (gravityCounter % GRAVITY == 0)
    topLED--;

  if (topLED > 0) {
    leds[NUM_LEDS/2-topLED] = ColorFromPalette(gCurrentPalette, millis(), 255);                       // LED falls when the volume goes down.
    leds[topLED+NUM_LEDS/2] = ColorFromPalette(gCurrentPalette, millis(), 255);                       // LED falls when the volume goes down.
  }
  
  gravityCounter = (gravityCounter + 1) % GRAVITY;
}

#define qsuba(x, b)  ((x>b)?x-b:0)                            // Unsigned subtraction macro. if result <0, then => 0.
#define qsubd(x, b)  ((x>b)?x:0)                              // A digital unsigned subtraction macro. if result <0, then => 0. Otherwise, take on fixed value.

void onesine() {
// Persistent local variable.
  static int thisphase = 0;                                                     // Phase change value gets calculated.

// Temporary local variables
  uint8_t allfreq = 32;                                                         // You can change the frequency, thus distance between bars. Wouldn't recommend changing on the fly.
  uint8_t thiscutoff;                                                            // You can change the cutoff value to display this wave. Lower value = longer wave.

  getSample();                                                                // Sample sound, measure averages and detect peak.
  agcAvg();

  thiscutoff = 255 - sampleAgc;
  
  thisphase += sampleAvg/2 + beatsin16(20,-10,10);                              // Move the sine waves along as a function of sound plus a bit of sine wave.

  for (int k=0; k<NUM_LEDS; k++) {                                              // For each of the LED's in the strand, set a brightness based on a wave as follows:
    int thisbright = qsubd(cubicwave8((k*allfreq)+thisphase), thiscutoff);      // qsub sets a minimum value called thiscutoff. If < thiscutoff, then bright = 0. Otherwise, bright = 128 (as defined in qsub)..

    leds[k] = ColorFromPalette( gCurrentPalette, millis()/2, thisbright);    // Let's now add the foreground colour. By Andrew Tuline.
  }
}

void pixelblend() {
// Persistent local variables
  static uint16_t currLED;                                                      // Persistent local value to count the current LED location.

  getSample();                                                                // Sample sound, measure averages and detect peak.
  agcAvg();

  currLED = (currLED+1)  % (NUM_LEDS);                                           // Cycle through all the LED's. By Andrew Tuline.

  CRGB newcolour = ColorFromPalette(gCurrentPalette, sin8(sample), sampleAgc);   // Colour of the LED will be based on the sample, while brightness is based on sampleavg.
  nblend(leds[currLED], newcolour, 192);                                        // Blend the old value and the new value for a gradual transitioning.
}

void plasma() {
// Persistent local variables
  static int16_t thisphase = 0;                                                 // Phase of a cubicwave8.
  static int16_t thatphase = 0;                                                 // Phase of the cos8.

// Temporary local variables
  uint16_t thisbright;
  uint16_t colorIndex;

  getSample();                                                                // Sample sound, measure averages and detect peak.
  agcAvg();

  thisphase += beatsin8(6,-4,4);                                                // You can change direction and speed individually.
  thatphase += beatsin8(7,-4,4);                                                // Two phase values to make a complex pattern. By Andrew Tuline.

  for (int k=0; k<NUM_LEDS; k++) {                                              // For each of the LED's in the strand, set a brightness based on a wave as follows.
    thisbright = cubicwave8((k*8)+thisphase)/2;    
    thisbright += cos8((k*10)+thatphase)/2;                                     // Let's munge the brightness a bit and animate it all with the phases.
    colorIndex=thisbright;
    thisbright = qsuba(thisbright, 255 - sampleAvg*4);                    // qsuba chops off values below a threshold defined by sampleAvg. Gives a cool effect.
 
    leds[k] += ColorFromPalette( gCurrentPalette, colorIndex, thisbright);   // Let's now add the foreground colour.
  }

  fadeToBlackBy(leds, NUM_LEDS, 64); 
}

void plasma2() {                                                 // This is the heart of this program. Sure is short. . . and fast.
  int thisPhase = beatsin8(6,-64,64);                           // Setting phase change for a couple of waves.
  int thatPhase = beatsin8(7,-64,64);

  int thisFreq = beatsin8(8,10,20);
  int thatFreq = beatsin8(15,15,30);

  getSample();                                                                // Sample sound, measure averages and detect peak.
  agcAvg();

  for (int k=0; k<NUM_LEDS; k++) {                              // For each of the LED's in the strand, set a brightness based on a wave as follows:

    int colorIndex = cubicwave8((k*23)+thisPhase)/2 + cos8((k*15)+thatPhase)/2;     // Create a wave and add a phase change and add another wave with its own phase change.. Hey, you can even change the frequencies if you wish, but don't change on the fly.
    int thisBright = qsuba(colorIndex, beatsin8(7,0,96));                                 // qsub gives it a bit of 'black' dead space by setting sets a minimum value. If colorIndex < current value of beatsin8(), then bright = 0. Otherwise, bright = colorIndex..
    leds[k] = ColorFromPalette(gCurrentPalette, colorIndex, sin8(sampleAvg*k));  // Let's now add the foreground colour.
  }
}

void rainbowpeak() {
// Temporary local variables
  uint8_t beatA = beatsin8(17, 0, 255);                                           // Starting hue.

  getSample();                                                                // Sample sound, measure averages and detect peak.
  agcAvg();
  
  if (samplePeak) {                                                               // Trigger a rainbow with a peak.
    samplePeak = 0;                                                               // Got a peak, now reset it.
    uint8_t locn = random8(0,NUM_LEDS);
    fill_palette(leds + locn, random8(0,(NUM_LEDS-locn)), beatA, 8, gCurrentPalette, 255, LINEARBLEND);
  }
  
  fadeToBlackBy(leds, NUM_LEDS, 40);                                              // Fade everything. By Andrew Tuline.
}

void ripple() {                                                                 // Display ripples triggered by peaks.
// Local definitions
  #define maxsteps 16                                                           // Maximum number of steps.

// Persistent local variables
  static uint8_t colour;                                                        // Ripple colour is based on samples.
  static uint16_t center = 0;                                                   // Center of current ripple.
  static int8_t step = -1;                                                      // Phase of the ripple as it spreads out.

  getSample();                                                                // Sample sound, measure averages and detect peak.
  agcAvg();

  if (samplePeak) {samplePeak = 0; step = -1;}                                  // Trigger a new ripple if we have a peak.
  
  fadeToBlackBy(leds, NUM_LEDS, 64);                                            // Fade the strand, where 1 = slow, 255 = fast

  switch (step) {
    case -1:                                                                    // Initialize ripple variables. By Andrew Tuline.
      center = random(NUM_LEDS);
      colour = (sample) % 255;                                                  // More peaks/s = higher the hue colour.
      step = 0;
      break;

    case 0:
      leds[center] += ColorFromPalette(gCurrentPalette, colour+millis(), 255); // Display the first pixel of the ripple.
      step ++;
      break;

    case maxsteps:                                                              // At the end of the ripples.
      // step = -1;
      break;

    default:                                                                    // Middle of the ripples.

      leds[(center + step + NUM_LEDS) % NUM_LEDS] += ColorFromPalette(gCurrentPalette, colour+millis(), 255/step*2);  // A spreading and fading pattern up the strand.
      leds[(center - step + NUM_LEDS) % NUM_LEDS] += ColorFromPalette(gCurrentPalette, colour+millis(), 255/step*2);  // A spreading and fading pattern down the strand.
      step ++;                                                                  // Next step.
      break;  
      
  } // switch step
}

void sinephase() {
  getSample();                                                                // Sample sound, measure averages and detect peak.
  agcAvg();

  for (int i=0; i<NUM_LEDS; i++) {

    int hue = sampleAvg*2 + sin8(i*4+beatsin16(13,-20,50));
    int bri = hue;
    bri = bri*bri/255;
    leds[i] = ColorFromPalette(gCurrentPalette, hue, bri);
  }
}

void soundGradient()
{
  getSample();                                                                // Sample sound, measure averages and detect peak.
  agcAvg();

  uint8_t tempsamp = constrain(sampleAvg*2,0,255);                     // Keep the sample from overflowing.
  FastLED.setBrightness(tempsamp);
  
  gHue = (int) gHue + speed / 20;
  fill_palette( leds, NUM_LEDS, gHue, 6, gCurrentPalette, 255, LINEARBLEND);
}
