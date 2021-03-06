// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include <SdFat.h>
#include <LowPower.h>
#include <RTClib.h>
#include <avr/wdt.h>
#include <OneWire.h>
#include <DallasTemperature.h>


#define PIN_PWR         6
#define PIN_FAIL_LED    7
#define PIN_1WIRE       5
#define PIN_CS_SD       4
#define PIN_VBAT        A0
#define Vcc             3.3
#define RTC_MODEL       3231

#define DELAY_SECS      2

SdFat sd;
#if RTC_MODEL == 3231
RTC_DS3231 rtc;
#elif RTC_MODEL == 1307
RTC_DS1307 rtc;
#else
#error "RTC must be one of DS3231 or DS1307"
#endif

OneWire oneWire(PIN_1WIRE);
DallasTemperature sensors(&oneWire);

static volatile int failures = 0;

void iso8601(char *buf, const DateTime &t);
void mkfilename(char *buf, const DateTime &t);
void pwrDown();
bool pwrUp();
void deepSleep(int);
float readVbat();
// Reset by calling a null function pointer
void (*reset) (void) = NULL;


void setup() {
    Serial.begin(9600);
    Serial.println("# Initializing... ");

    failures = 0;

    pinMode(10, OUTPUT); // SS pin must be kept high
    pinMode(PIN_CS_SD, OUTPUT);
    pinMode(PIN_PWR, OUTPUT);
    pinMode(PIN_FAIL_LED, OUTPUT);

    digitalWrite(PIN_PWR, HIGH);
    delay(20);

    sensors.begin();

    if (!pwrUp()) {
        Serial.println("# Initializing failed");
        pwrDown();
        deepSleep(1);
        reset();
    }

    #if RTC_MODEL == 1307
    if (!rtc.isrunning()) {
    #elif RTC_MODEL == 3231
    if (rtc.lostPower()) {
    #endif
        Serial.println("# Couldn't find RTC");
        // Set clock if unset
        rtc.adjust(DateTime(F(__DATE__), F(__TIME__)));
    }

    Serial.println("# Initializing success");
    Serial.println("time\ttemperature\tvbat");

    pwrDown();
}

void loop() {
    deepSleep(DELAY_SECS);
    if (!pwrUp()) {
        pwrDown();
        return;
    }

    DateTime now = rtc.now();
    char ts[25] = "";
    iso8601(ts, now);

    char filename[32] = "glb.tsv";
    //mkfilename(filename, now);

    SdFile log_file;
    if (!log_file.open(filename, FILE_WRITE)) {
        Serial.println("# error opening file");
        pwrDown();
        return;
    }

    sensors.requestTemperatures();
    Serial.println();
    float t = sensors.getTempCByIndex(0);
    float vbat = readVbat();

    log_file.print(ts);
    log_file.print("\t");
    log_file.print(t);
    log_file.print("\t");
    log_file.println(vbat);
    log_file.sync();
    log_file.close();

    Serial.print(ts);
    Serial.print("\t");
    Serial.print(t);
    Serial.print("\t");
    Serial.println(vbat);

    // Power off
    failures = 0;
    digitalWrite(PIN_FAIL_LED, LOW);
    pwrDown();
}

float readVbat()
{
    return analogRead(PIN_VBAT) * (Vcc / 1024.0) * 2;
}

void deepSleep(int to_sleep)
{
    // To be replaced with RTC interrupt and SLEEP_FOREVER
    while (to_sleep >= 8) {
        LowPower.powerDown(SLEEP_8S, ADC_OFF, BOD_OFF);
        to_sleep -= 8;
    }
    while (to_sleep > 0) {
        LowPower.powerDown(SLEEP_1S, ADC_OFF, BOD_OFF);
        to_sleep -= 1;
    }
}

void pwrDown()
{
    Serial.flush(); Serial.end();
    digitalWrite(PIN_PWR, LOW);
}

bool pwrUp()
{
    Serial.begin(9600);
    digitalWrite(PIN_PWR, HIGH);
    delay(10); // Wait for pwr on

    if (failures >= 10) {
        reset();
    }
    if (failures > 0) {
        digitalWrite(PIN_FAIL_LED, HIGH);
    }

    if (!sd.begin(PIN_CS_SD, SPI_HALF_SPEED)) {
        Serial.println("Card init failed");
        failures++;
        return false;
    }

    if (!rtc.begin()) {
        Serial.println("Couldn't find RTC");
        failures++;
        return false;
    }

    // Reset if the RTC isn't running
    #if RTC_MODEL == 1307
    bool needs_setting = !rtc.isrunning();
    #elif RTC_MODEL == 3231
    bool needs_setting = rtc.lostPower();
    #endif
    if (needs_setting) reset();


    // Allow systems to stabilise before starting measurements
    delay(10);
    return true;
}

void iso8601(char *buffer, const DateTime &t)
{
    int len = sprintf(buffer, "%04u-%02u-%02u_%02u:%02u:%02u", t.year(), t.month(),
            t.day(), t.hour(), t.minute(), t.second());
    buffer[len] = 0;
}

void mkfilename(char *buffer, const DateTime &t)
{
    int len = sprintf(buffer, "%04u-%02u-%02u.tsv", t.year(), t.month(), t.day());
    buffer[len] = 0;
}
