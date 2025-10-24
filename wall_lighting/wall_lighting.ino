#define FASTLED_ALLOW_INTERRUPTS 0
#include <FastLED.h>

#include <WiFi.h>
#include <WiFiUdp.h>  // For NTP
#include <NTPClient.h>
#include <Timezone.h>  // https://github.com/JChristensen/Timezone

#include <Firebase_ESP_Client.h>
#include <addons/TokenHelper.h>  // token generation process info
#include <addons/RTDBHelper.h>   // RTDB payload printing info and helper functions

#include "matrix.h"
#include "structs.h"
#include "secrets.h"

FASTLED_USING_NAMESPACE

#define DATA_PIN 21
#define NUM_LEDS 64
#define LED_TYPE WS2812B
#define COLOR_ORDER GRB

// #define Y1 4  // for testing
#define Y1 18
#define Y2 19
#define Y3 16

#define Y1_NUM 16
#define Y2_NUM 24
#define Y3_NUM 39

#define RING1 26
#define RING2 5
#define RING3 23

#define ENTERPRISE

CRGB leds[NUM_LEDS];
CRGB ledY1[Y1_NUM];
CRGB ledY2[Y2_NUM];
CRGB ledY3[Y3_NUM];

CLEDController *mainlamp;

WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP);

TimeChangeRule etDST = { "EDT", Second, Sun, Mar, 2, -240 };  // Daylight time = UTC - 4 hours
TimeChangeRule etSTD = { "EST", First, Sun, Nov, 2, -300 };   // Standard time = UTC - 5 hours
Timezone et(etDST, etSTD);

// Define Firebase Data object
FirebaseData fbdo;

FirebaseAuth auth;
FirebaseConfig config;

Roomdata data;
LinearAnimation la;

bool brightnessOverride = false;
bool ringlampOverride = false;
bool ioOverride = false;
bool onetimeHandler;

bool bUp = false;
bool bDown = false;
bool rlCycle = false;
bool custom = false;
int touch_threshold = 40;

long cycleDelay;

void bUpISR() {
  brightnessOverride = true;
  if (data.brightness < 255) data.brightness++;  // don't want it to cycle back to zero
  bUp = true;                                    // just in case you want to get the touch event for something else
}

void bDownISR() {
  brightnessOverride = true;
  if (data.brightness > 0) data.brightness--;  // don't want it to cycle back to 255
  bDown = true;
}

void rlCycleISR() {
  data.devices.ringLamp = true;
  static long t = 0;
  ringlampOverride = true;
  if (millis() - t > 1000) {
    data.ringlamp++;
    t = millis();
  }
  if (data.ringlamp > 7) { data.ringlamp = 0; }
  rlCycle = true;
}

void customtouchISR() {
  ioOverride = true;
  data.devices.ringLamp != data.devices.ringLamp;
  data.devices.wallLeft != data.devices.wallLeft;
  data.devices.wallRight != data.devices.wallRight;
  data.devices.wordClock != data.devices.wordClock;
  data.devices.yNetwork != data.devices.yNetwork;
  // this mode switches the enabled state of all devices
  custom = true;  // do whatever with this, set it back to false once you've used it
}

void waveanimation() {
  // easier to type these inside the function
  int hue = data.colorscheme.hue;
  int sat = data.colorscheme.saturation;
  int var = data.colorscheme.variation;

  static int waveOffset1 = 0;
  static int waveOffset2 = 0;
  static int waveOffset3 = 0;
  static int waveOffset4 = 0;

  // give them a base color
  CRGB base;
  hsv2rgb_rainbow(CHSV(hue, sat, 40), base);

  fill_solid(ledY1, Y1_NUM, base);
  fill_solid(ledY2, Y2_NUM, base);
  fill_solid(ledY3, Y3_NUM, base);

  int bri;

  // there are three distinct arrays, each with a different number of elements. 
  // this means that each array needs to be added to separately. Hence, 12 for() loops.
  // each for loop adds a sine wave of a certain color intensity as follows: 
  //    the first set of three for loops add the specific color value "hue" in a sine wave
  //    the rest of the sets of for loops add a sine wave of hue plus some arbitrary amount of variation.
  //    this amount of variation was determined by what I thought looked nicest. 
  for (int i = 0; i < Y1_NUM; i++) {
    CRGB temp;
    bri = ((int)((float)data.colorscheme.brightness / 255.0 * (float)sin8((16 * i + waveOffset1))));
    hsv2rgb_rainbow(CHSV(hue, sat, bri), temp);
    ledY1[i] += temp / 4;
  }

  for (int i = 0; i < Y2_NUM; i++) {
    CRGB temp;
    bri = ((int)((float)data.colorscheme.brightness / 255.0 * (float)sin8((16 * i + waveOffset1))));
    hsv2rgb_rainbow(CHSV(hue, sat, bri), temp);
    ledY2[i] += temp / 4;
  }

  for (int i = 0; i < Y3_NUM; i++) {
    CRGB temp;
    bri = ((int)((float)data.colorscheme.brightness / 255.0 * (float)sin8((16 * i + waveOffset1))));
    hsv2rgb_rainbow(CHSV(hue, sat, bri), temp);
    ledY3[i] += temp / 4;
  }

  for (int i = 0; i < Y1_NUM; i++) {
    CRGB temp;
    bri = ((int)((float)data.colorscheme.brightness / 255.0 * (float)sin8((16 * i + waveOffset2))));
    hsv2rgb_rainbow(CHSV(qadd8(hue, var / 2), sat, bri), temp);
    ledY1[i] += temp / 4;
  }

  for (int i = 0; i < Y2_NUM; i++) {
    CRGB temp;
    bri = ((int)((float)data.colorscheme.brightness / 255.0 * (float)sin8((16 * i + waveOffset2))));
    hsv2rgb_rainbow(CHSV(qadd8(hue, var / 2), sat, bri), temp);
    ledY2[i] += temp / 4;
  }

  for (int i = 0; i < Y3_NUM; i++) {
    CRGB temp;
    bri = ((int)((float)data.colorscheme.brightness / 255.0 * (float)sin8((16 * i + waveOffset2))));
    hsv2rgb_rainbow(CHSV(qadd8(hue, var / 2), sat, bri), temp);
    ledY3[i] += temp / 4;
  }

  for (int i = 0; i < Y1_NUM; i++) {
    CRGB temp;
    bri = ((int)((float)data.colorscheme.brightness / 255.0 * (float)sin8((16 * i + waveOffset3))));
    hsv2rgb_rainbow(CHSV(qsub8(hue, var / 3), sat, bri), temp);
    ledY1[i] += temp / 4;
  }

  for (int i = 0; i < Y2_NUM; i++) {
    CRGB temp;
    bri = ((int)((float)data.colorscheme.brightness / 255.0 * (float)sin8((16 * i + waveOffset3))));
    hsv2rgb_rainbow(CHSV(qsub8(hue, var / 3), sat, bri), temp);
    ledY2[i] += temp / 4;
  }

  for (int i = 0; i < Y3_NUM; i++) {
    CRGB temp;
    bri = ((int)((float)data.colorscheme.brightness / 255.0 * (float)sin8((16 * i + waveOffset3))));
    hsv2rgb_rainbow(CHSV(qsub8(hue, var / 3), sat, bri), temp);
    ledY3[i] += temp / 4;
  }

  for (int i = 0; i < Y1_NUM; i++) {
    CRGB temp;
    bri = ((int)((float)data.colorscheme.brightness / 255.0 * (float)sin8((16 * i + waveOffset4))));
    hsv2rgb_rainbow(CHSV(qsub8(hue, var / 3), sat, bri), temp);
    ledY1[i] += temp / 4;
  }

  for (int i = 0; i < Y2_NUM; i++) {
    CRGB temp;
    bri = ((int)((float)data.colorscheme.brightness / 255.0 * (float)sin8((16 * i + waveOffset4))));
    hsv2rgb_rainbow(CHSV(qsub8(hue, var / 3), sat, bri), temp);
    ledY2[i] += temp / 4;
  }

  for (int i = 0; i < Y3_NUM; i++) {
    CRGB temp;
    bri = ((int)((float)data.colorscheme.brightness / 255.0 * (float)sin8((16 * i + waveOffset4))));
    hsv2rgb_rainbow(CHSV(qsub8(hue, var / 3), sat, bri), temp);
    ledY3[i] += temp / 4;
  }

  waveOffset1 += beatsin8(15, 2, 4);
  waveOffset2 += beatsin8(10, 2, 4);
  waveOffset3 += beatsin8(20, 2, 4);
  waveOffset4 += beatsin8(6, 2, 4);
}


void setup() {
  pinMode(RING1, OUTPUT);
  pinMode(RING2, OUTPUT);
  pinMode(RING3, OUTPUT);

  analogWrite(RING1, 255);
  analogWrite(RING2, 255);
  analogWrite(RING3, 255);

  Serial.begin(112500);
  vTaskDelay(50);

  touchAttachInterrupt(4, bUpISR, touch_threshold);
  touchAttachInterrupt(27, bDownISR, touch_threshold);
  touchAttachInterrupt(32, rlCycleISR, touch_threshold);
  touchAttachInterrupt(33, customtouchISR, touch_threshold);

  data.brightness = 255;
  data.colorscheme.brightness = 155;
  data.colorscheme.variation = 30;
  data.colorscheme.hue = 136;
  data.colorscheme.saturation = 255;
  data.devices.wordClock = true;

  mainlamp = &FastLED.addLeds<LED_TYPE, DATA_PIN, COLOR_ORDER>(leds, NUM_LEDS);
  FastLED.addLeds<LED_TYPE, Y1, COLOR_ORDER>(ledY1, Y1_NUM);
  FastLED.addLeds<LED_TYPE, Y2, COLOR_ORDER>(ledY2, Y2_NUM);
  FastLED.addLeds<LED_TYPE, Y3, COLOR_ORDER>(ledY3, Y3_NUM);
  FastLED.setBrightness(data.brightness);
  Serial.printf("brightness: %s\n", String(FastLED.getBrightness()).c_str());

  WiFi.mode(WIFI_STA);
  Serial.printf("Connecting to %s", WIFI_SSID);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

  while (WiFi.status() != WL_CONNECTED) {
    Serial.printf(".");
    delay(300);
    if (millis() > 10000) {
      ESP.restart();
    }
  }

  timeClient.begin();

  Serial.printf("Firebase Client v%s\n\n", FIREBASE_CLIENT_VERSION);

  /* Assign the api key (required) */
  config.api_key = API_KEY;

  /* Assign the user sign in credentials */
  auth.user.email = USER_EMAIL;
  auth.user.password = USER_PASSWORD;

  /* Assign the RTDB URL (required) */
  config.database_url = DATABASE_URL;

  /* Assign the callback function for the long running token generation task */
  config.token_status_callback = tokenStatusCallback;  // see addons/TokenHelper.h

  // Since Firebase v4.4.x, BearSSL engine was used, the SSL buffer need to be set.
  // Large data transmission may require larger RX buffer, otherwise connection issue or data read time out can be occurred.
  fbdo.setBSSLBufferSize(2048 /* Rx buffer size in bytes from 512 - 16384 */, 1024 /* Tx buffer size in bytes from 512 - 16384 */);

  // Or use legacy authenticate method
  config.database_url = DATABASE_URL;
  config.signer.tokens.legacy_token = "<database secret>";

  Firebase.begin(&config, &auth);

  // Comment or pass false value when WiFi reconnection will control by your code or third party library e.g. WiFiManager
  Firebase.reconnectNetwork(false);
}

void printTime(long t) {
  Serial.printf("%s:%s:%s\n", String(hour(now())).c_str(), String(minute(now())).c_str(), String(second(now())).c_str());
}

void loop() {
  static long varsUpdate = 59999;
  EVERY_N_MILLISECONDS(1000) {
    Serial.printf("brightness override: %s ring lamp override: %s: io override: %s\t", String(brightnessOverride).c_str(), String(ringlampOverride).c_str(), String(ioOverride).c_str());
    printTime(now());
    clock(et.toLocal(now()));
  }
  if (millis() - varsUpdate > 60000) {  // update once per minute
    if (!(ioOverride || brightnessOverride || ringlampOverride)) {
      if (Firebase.ready()) {  // makes sure FB ready to get data
        timeClient.update();
        setTime(timeClient.getEpochTime());
        Serial.print(timeClient.getFormattedTime());
        getVars();  // only get if ready    definitely inefficient but I'm lazy

        // This order of get/set vars works better than set
        // then getbecause it gets the key times first, although
        // updates to the actual lamp itself are delayed by a minute.

        varsUpdate = millis();  // only update this if ready
      }
    }
  }
  EVERY_N_MILLISECONDS(20) {
    waveanimation();
    ringlamp();
    FastLED.setBrightness(data.brightness);
    FastLED.show();
  }

  if ((now() >= data.keytimes.wake - 1800 && now() < data.keytimes.wake - 1500) && !onetimeHandler) {
    onetimeHandler = true;
    sunrise();
  }
  if ((now() >= data.keytimes.bed - 1200 && now() < data.keytimes.bed - 900) && !onetimeHandler) {
    onetimeHandler = true;
    sunset();
  }
  if (now() > data.keytimes.wake - 1500 || now() > data.keytimes.bed - 900) { onetimeHandler = false; }

  if (bUp) {
    Serial.println(F("bup"));
    bUp = false;
  }
  if (bDown) {
    Serial.println(F("bdown"));
    bDown = false;
  }
  if (rlCycle) {
    Serial.printf("rlcycle %s\n", String(data.ringlamp).c_str());
  }
  if (custom) {
    Serial.println(F("custom"));
    custom = false;
  }
}

/*inline int linear(int start, int end, int totaltime, int t) {
  double s = start;
  double e = end;
  double tt = totaltime;
  return (int)(((e - s) / tt) * t + start);
}

void linearanimation_ms(void *ln) {
  LinearAnimation la = *(LinearAnimation *)ln;
  long start = millis();

  while (millis() - start < la.animationtime) {
    data.colorscheme.hue = linear(la.startHue, la.endHue, la.animationtime, millis() - start);
    data.colorscheme.saturation = linear(la.startSat, la.endSat, la.animationtime, millis() - start);
    data.brightness = linear(la.startVib, la.endVib, la.animationtime, millis() - start);
    vTaskDelay(5);  // other tasks are important!
  }

  vTaskDelete(NULL);  // clear space
  la.indicator = true;
}

void linearanimationreverse_ms(void *ln) {
  LinearAnimation la = *(LinearAnimation *)ln;
  long start = millis();
  int firstSize;
  int secondSize;
  if (la.startHue > la.endHue) {
    firstSize = la.startHue;
    secondSize = 255 - la.endHue;
  } else {
    firstSize = 255 - la.startHue;
    secondSize = la.endHue;
  }
  double fs = firstSize;
  double sec = secondSize;
  double at = la.animationtime;
  int firstTime = (int)(at * (fs / (fs + sec)));
  int midSat = (int)((la.endSat - la.startSat) * (fs / (fs + sec))) + la.startSat;
  int midVib = (int)((la.endVib - la.startVib) * (fs / (fs + sec))) + la.startVib;
  int secondTime = la.animationtime - firstTime;
  LinearAnimation lf;
  if (la.startHue > la.endHue) {
    lf = { la.startHue, la.startSat, la.startVib, 255, midSat, midVib, firstTime, false };
  } else {
    lf = { la.startHue, la.startSat, la.startVib, 0, midSat, midVib, firstTime, false };
  }
  xTaskCreate(linearanimation_ms, "Linear animation reverse (ms) 1", 1024, &lf, 1, NULL);
  while (!lf.indicator) { vTaskDelay(10); }
  if (la.startHue > la.endHue) {
    lf = { 0, midSat, midVib, la.endHue, la.endSat, la.endVib, secondTime, false };
  } else {
    lf = { 255, midSat, midVib, la.endHue, la.endSat, la.endVib, secondTime, false };
  }
  xTaskCreate(linearanimation_ms, "Linear animation reverse (ms) 2", 1024, &lf, 1, NULL);
  while (!lf.indicator) { vTaskDelay(10); }
  la.indicator = true;
  vTaskDelete(NULL);
}

void linearanimationreverse_s(void *ln) {
  LinearAnimation la = *(LinearAnimation *)ln;
  la.animationtime *= 1000;
  xTaskCreate(linearanimationreverse_ms, "Linear animation reverse (s)", 2048, &la, 1, NULL);
  vTaskDelete(NULL);
}

void linearanimation_s(void *ln) {
  LinearAnimation la = *(LinearAnimation *)ln;
  la.animationtime *= 1000;
  xTaskCreate(linearanimation_ms, "Linear animation (s)", 2048, &la, 1, NULL);
  vTaskDelete(NULL);
}

void sunrise() {
  xTaskCreate(sunrise, "sunrise", 1536, NULL, 1, NULL);
}

void sunrise(void *pvParams) {
  int hue = data.colorscheme.hue;
  LinearAnimation la = { hue, 245, 0, 170, 35, 255, 1800, false };
  xTaskCreate(linearanimation_s, "sunrise 1", 1024, &la, 1, NULL);
  while (!la.indicator) {
    vTaskDelay(10);
  }
  la = { 170, 35, 255, 170, 115, 245, 300, false };
  xTaskCreate(linearanimation_s, "sunrise 2", 1024, &la, 1, NULL);
  while (!la.indicator) {
    vTaskDelay(10);
  }
  la = { 170, 115, 245, hue, 255, 255, 30, false };
  xTaskCreate(linearanimation_s, "sunrise 3", 1024, &la, 1, NULL);
  while (!la.indicator) {
    vTaskDelay(10);
  }
  vTaskDelay(10);
  data.colorscheme.hue = hue;
  vTaskDelete(NULL);
}

void sunset() {
  xTaskCreate(sunset, "sunset", 1536, NULL, 1, NULL);
}

void sunset(void *pvParams) {
  int hue = data.colorscheme.hue;
  LinearAnimation la = { hue, 255, 255, 220, 170, 255, 60, false };
  xTaskCreate(linearanimation_s, "sunset 1", 1024, &la, 1, NULL);
  while (!la.indicator) {
    vTaskDelay(10);
  }
  la = { 220, 170, 255, 25, 200, 200, 420, false };
  xTaskCreate(linearanimationreverse_s, "sunrise 2", 1024, &la, 1, NULL);
  while (!la.indicator) {
    vTaskDelay(10);
  }
  la = { 25, 200, 200, 35, 230, 120, 360, false };
  xTaskCreate(linearanimationreverse_s, "sunrise 2", 1024, &la, 1, NULL);
  while (!la.indicator) {
    vTaskDelay(10);
  }
  la = { 35, 230, 120, 35, 255, 0, 360, false };
  xTaskCreate(linearanimation_s, "sunrise 3", 1024, &la, 1, NULL);
  while (!la.indicator) {
    vTaskDelay(10);
  }
  vTaskDelay(10);
  data.colorscheme.hue = hue;
  vTaskDelete(NULL);
}
*/

void ringlamp() {
  if (data.devices.ringLamp) {
    switch (data.ringlamp) {
      case 1:
        analogWrite(RING1, 255 - data.brightness);
        analogWrite(RING2, 255);
        analogWrite(RING3, 255);
        break;
      case 2:
        analogWrite(RING1, 255);
        analogWrite(RING2, 255 - data.brightness);
        analogWrite(RING3, 255);
        break;
      case 3:
        analogWrite(RING1, 255 - data.brightness);
        analogWrite(RING2, 255 - data.brightness);
        analogWrite(RING3, 255);
        break;
      case 4:
        analogWrite(RING1, 255);
        analogWrite(RING2, 255);
        analogWrite(RING3, 255 - data.brightness);
        break;
      case 5:
        analogWrite(RING1, 255 - data.brightness);
        analogWrite(RING2, 255);
        analogWrite(RING3, 255 - data.brightness);
        break;
      case 6:
        analogWrite(RING1, 255);
        analogWrite(RING2, 255 - data.brightness);
        analogWrite(RING3, 255 - data.brightness);
        break;
      case 7:
        analogWrite(RING1, 255 - data.brightness);
        analogWrite(RING2, 255 - data.brightness);
        analogWrite(RING3, 255 - data.brightness);
        break;
      default:
        analogWrite(RING1, 255);
        analogWrite(RING2, 255);
        analogWrite(RING3, 255);
        break;
    }
  } else {
    analogWrite(RING1, 255);
    analogWrite(RING2, 255);
    analogWrite(RING3, 255);
  }
}

void getVars() {
  // update these because the ESP controls those three unless button has overridden them
  Serial.println(F("getvarscall"));
  if (!ioOverride) {
    Firebase.RTDB.getBool(&fbdo, F("/room/activeDevices/Word clock"), &data.devices.wordClock);
    Firebase.RTDB.getBool(&fbdo, F("/room/activeDevices/Ring lamp"), &data.devices.ringLamp);
    Firebase.RTDB.getBool(&fbdo, F("/room/activeDevices/Y network"), &data.devices.yNetwork);
  }
  if (!ringlampOverride) {
    Firebase.RTDB.getInt(&fbdo, F("/room/ring"), &data.ringlamp);
  }
  if (!brightnessOverride) {
    int bVal;
    Firebase.RTDB.getInt(&fbdo, F("/room/brightness"), &bVal);
    data.brightness = bVal;
  }
  Serial.printf("Get brightness... %s\t", String(data.brightness).c_str());


  // getting key times to set device activity (won't set if button override)
  int iVal;
  Serial.printf("Get wake... %s\n", Firebase.RTDB.getInt(&fbdo, FPSTR("/room/keyTimes/wakeup"), &iVal) ? String(iVal).c_str() : fbdo.errorReason().c_str());
  Firebase.RTDB.getInt(&fbdo, F("/room/keyTimes/wakeup"), &data.keytimes.wake);
  Firebase.RTDB.getInt(&fbdo, F("/room/keyTimes/leave"), &data.keytimes.leave);
  Firebase.RTDB.getInt(&fbdo, F("/room/keyTimes/return"), &data.keytimes.ret);
  Firebase.RTDB.getInt(&fbdo, F("/room/keyTimes/winddown"), &data.keytimes.winddown);
  Firebase.RTDB.getInt(&fbdo, F("/room/keyTimes/bed"), &data.keytimes.bed);

  // get color scheme
  Firebase.RTDB.getInt(&fbdo, F("/room/colorScheme/variation"), &iVal);
  data.colorscheme.variation = iVal;
  Serial.printf("Get variation... %s\t", String(data.colorscheme.variation).c_str());

  Firebase.RTDB.getInt(&fbdo, F("/room/colorScheme/brightness"), &iVal);
  data.colorscheme.brightness = iVal * 255 / 360;  // convert from 360 degree standard HSV to 255 "degree" (2 byte) FastLED HSV
  Serial.printf("Get color brightness... %s\t", String(data.colorscheme.brightness).c_str());

  Firebase.RTDB.getInt(&fbdo, F("/room/colorScheme/saturation"), &iVal);
  data.colorscheme.brightness = iVal * 255 / 360;  // convert from 360 degree standard HSV to 255 "degree" (2 byte) FastLED HSV
  Serial.printf("Get saturation... %s\t", String(data.colorscheme.saturation).c_str());

  Firebase.RTDB.getInt(&fbdo, F("/room/colorScheme/hue"), &iVal);
  data.colorscheme.hue = iVal;
  Serial.printf("Get hue... %s\n", String(data.colorscheme.hue).c_str());

  Serial.printf("Get brightness... %s\t", String(data.brightness).c_str());
}

void clock(time_t t) {
  if (!data.devices.wordClock) return;
  if (t < 10) {
    brightnessOverride = false;
    ringlampOverride = false;
    ioOverride = false;
  }  // resets button overrides at midnight (does it like 10 times to be safe)
     // does it in clock() because it is called more often than WiFi functions

  int h = hour(t) % 12;
  int m = minute(t);

  fill_solid(leds, NUM_LEDS, 0);

  // 0 - 8: past dot
  // 9 - 22: quarter past
  // 23 - 38: half past
  // 39 - 52: quarter to
  // 53 - 0: zero to

  if (m <= 8) {
    writeMatrix(ZERO);
    writeMatrix(PAST);
  } else if (m <= 22) {
    writeMatrix(AQUARTER);
    writeMatrix(PAST);
  } else if (m <= 38) {
    writeMatrix(HALF);
    writeMatrix(PAST);
  } else if (m <= 52) {
    writeMatrix(AQUARTER);
    writeMatrix(TO);
    h++;
  } else {
    writeMatrix(ONE);
    writeMatrix(TO);
    h++;
  }

  if (h == 0) { h = 12; }
  writeMatrix((Words)h);

  switch (data.brightness) {
    case 0:
      mainlamp->showLeds(0);
      break;
    default:
      mainlamp->showLeds(255);  // because otherwise it's hard to see through cover
      break;
  }
}

void writeMatrix(Words word) {
  CRGB color;
  hsv2rgb_rainbow(CHSV(data.colorscheme.hue, data.colorscheme.saturation, data.colorscheme.brightness), color);
  writeMatrix(word, color);
}

void writeMatrix(int x, int y, CRGB color) {
  if (!data.devices.wordClock) return;
  int pos = 8 * (7 - x);
  if (x % 2) {
    pos += y;
  } else {
    pos += (7 - y);
  }
  if (0 > pos || 64 <= pos) { return; }
  leds[pos].green = color.green;
  leds[pos].red = color.red;
  leds[pos].blue = color.blue;
}

void writeMatrix(Words word, CRGB color) {
  for (int i = 0; i < numpixels[word]; i++) {
    writeMatrix(matrix[word][i].x, matrix[word][i].y, color);
  }
}