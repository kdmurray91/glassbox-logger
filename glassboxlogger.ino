// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include <DHT.h>
#include <SD.h>
#include <LowPower.h>
#include <RTClib.h>


// Uncomment the following during final/non-debugging builds
#define GLB_PRODUCTION

#define PIN_DHT     2
#define PIN_DHT2    3
#define PIN_CS_SD   4
#define PIN_PWR     9
#define PIN_LED     13

#define LOG_FILENAME "temp.tsv"
#define DELAY_SECS  30
#define RTC_CLASS RTC_DS1307

/* WIRING
# DHT
Connect pin 1 (on the left) of the sensor to +5V (or 3v3 if logic is 3v3)
Connect pin 2 of the sensor to PIN_DHT
Connect pin 4 (on the right) of the sensor to GROUND

# SD card
For the etherten: Pin is pin4
Otherwise, connect SCL, MISO, MOSI to (, and CS/SS to PIN_CS_SD
*/


RTC_CLASS rtc;
DHT dht(PIN_DHT, DHT22);
DHT dht2(PIN_DHT2, DHT22);

void startBlink();
void iso8601(char *buf, const DateTime &t);

void setup() {
    #ifndef GLB_PRODUCTION
    Serial.begin(9600);
    Serial.print("#Initializing... ");
    #endif

    pinMode(PIN_CS_SD, OUTPUT);
    pinMode(PIN_PWR, OUTPUT);
    pinMode(PIN_LED, OUTPUT);

    digitalWrite(PIN_LED, HIGH);
    digitalWrite(PIN_PWR, HIGH);


    if (!SD.begin(PIN_CS_SD)) {
        #ifndef GLB_PRODUCTION
        Serial.println("SD card FAILED -- suspending");
        #endif
        while(1);
    }

    if (!rtc.begin()) {
        #ifndef GLB_PRODUCTION
        Serial.println("Couldn't find RTC");
        #endif
        while (1);
    }
    if (! rtc.isrunning()) {
        // Set clock if unset
        rtc.adjust(DateTime(F(__DATE__), F(__TIME__)));
    }

    dht.begin();
    dht2.begin();

    if (!SD.exists(LOG_FILENAME)) {
        File log_file = SD.open(LOG_FILENAME, FILE_WRITE);
        if (!log_file) {
            #ifndef GLB_PRODUCTION
            Serial.println("# error opening file");
            #endif
            while(1);
        }
        log_file.println("time\thumidity_1\ttemerature_1\thumidity_2\ttemerature_2");
        log_file.close();
    }

    #ifndef GLB_PRODUCTION
    Serial.println("done!");
    Serial.println("time\thumidity_1\ttemerature_1\thumidity_2\ttemerature_2");
    #endif

    digitalWrite(PIN_LED, LOW);
    digitalWrite(PIN_PWR, LOW);
}

void loop() {
    uint32_t start = millis();
    float secs = (float)start / 1000.0;
    digitalWrite(PIN_PWR, HIGH);
    delay(10); // Wait for pwr on

    DateTime now = rtc.now();
    char ts[25] = "";
    iso8601(ts, now);


    File log_file = SD.open(LOG_FILENAME, FILE_WRITE);
    if (!log_file) {
#ifndef GLB_PRODUCTION
        Serial.println("# error opening file");
        delay(1000);
#endif
        return;
    }

    float h = dht.readHumidity();
    float t = dht.readTemperature();
    float h2 = dht2.readHumidity();
    float t2 = dht2.readTemperature();

    log_file.print(ts);
    log_file.print("\t");
    log_file.print(h);
    log_file.print("\t");
    log_file.print(t);
    log_file.print("\t");
    log_file.print(h2);
    log_file.print("\t");
    log_file.println(t2);
    log_file.close();

#ifndef GLB_PRODUCTION
    Serial.print(ts);
    Serial.print("\t");
    Serial.print(h);
    Serial.print("\t");
    Serial.print(t);
    Serial.print("\t");
    Serial.print(h2);
    Serial.print("\t");
    Serial.println(t2);
#endif

    // Power off
    digitalWrite(PIN_PWR, LOW);
    // To be replaced with RTC interrupt and SLEEP_FOREVER
    for (unsigned i = 0; i < DELAY_SECS; i++) {
        LowPower.powerDown(SLEEP_1S, ADC_OFF, BOD_OFF);
    }
}

/* Long blink, short blink, pause, 3 times
 */
void startBlink()
{
    pinMode(PIN_LED, OUTPUT);
    for (unsigned i = 0; i < 3; i++) {
        digitalWrite(PIN_LED, HIGH);
        delay(500);
        digitalWrite(PIN_LED, LOW);
        delay(500);
        digitalWrite(PIN_LED, HIGH);
        delay(100);
        digitalWrite(PIN_LED, LOW);
        delay(900);
    }
}

void pwrDown()
{
}

void iso8601(char *buffer, const DateTime &t)
{
    sprintf(buffer, "%04u-%02u-%02u_%02u:%02u:%02u", t.year(), t.month(), t.day(), t.hour(), t.minute(), t.second());
}
