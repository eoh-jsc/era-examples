/*************************************************************
  Download latest ERa library here:
    https://github.com/eoh-jsc/era-lib/releases/latest
    https://www.arduino.cc/reference/en/libraries/era
    https://registry.platformio.org/libraries/eoh-ltd/ERa/installation

    ERa website:                https://e-ra.io
    ERa blog:                   https://iotasia.org
    ERa forum:                  https://forum.eoh.io
    Follow us:                  https://www.fb.com/EoHPlatform
 *************************************************************
  Dashboard setup (DHT):
    Virtual Pin setup:
        V0, type: number
        V1, type: number

  Dashboard setup (Terminal Box):
    Virtual Pin setup:
        V2, type: string
        V3, type: string

    Terminal Box widget setup:
        From datastream: V2
        To datastream:   V3
        Action: the action of V2

    This example need Adafruit DHT sensor libraries:
        https://github.com/adafruit/Adafruit_Sensor
        https://github.com/adafruit/DHT-sensor-library
 *************************************************************/

// Enable debug console
// Set CORE_DEBUG_LEVEL = 3 first
// #define ERA_DEBUG
// #define ERA_SERIAL Serial

/* Select ERa host location (VN: Viet Nam, SG: Singapore) */
#define ERA_LOCATION_VN
// #define ERA_LOCATION_SG

// You should get Auth Token in the ERa App or ERa Dashboard
#define ERA_AUTH_TOKEN "ERA2706"

#include <Arduino.h>
#include <ERa.hpp>
#include <DHT.h>
#include <Widgets/ERaWidgets.hpp>
#include <Time/ERaEspTime.hpp>

/* Digital pin connected to sensor */
#define DHT_PIN 5

/* Uncomment type using! */
#define DHT_TYPE DHT11
// #define DHT_TYPE DHT21
// #define DHT_TYPE DHT22

const char ssid[] = "YOUR_SSID";
const char pass[] = "YOUR_PASSWORD";

/* DHT */
DHT dht(DHT_PIN, DHT_TYPE);

/* Terminal Box */
ERaString estr;
ERaWidgetTerminalBox terminal(estr, V2, V3);

/* NTP Time */
ERaEspTime syncTime;
TimeElement_t ntpTime;

/* This function will run every time ERa is connected */
ERA_CONNECTED() {
    ERA_LOG("ERa", "ERa connected!");
}

/* This function will run every time ERa is disconnected */
ERA_DISCONNECTED() {
    ERA_LOG("ERa", "ERa disconnected!");
}

/* This function get NTP time */
void getNtpTime() {
    syncTime.getTime(ntpTime);
    ERA_LOG(ERA_PSTR("NTP"), ERA_PSTR("%02d:%02d:%02dT%02d-%02d-%04dW%d"),
                             ntpTime.hour, ntpTime.minute, ntpTime.second,
                             ntpTime.day, ntpTime.month, ntpTime.year + 1970, ntpTime.wDay);
}

/* This function print uptime every second */
void timerEvent() {
    // Get NTP time
    getNtpTime();

    // Read humidity and temperature
    float humidity = dht.readHumidity();
    float temperature = dht.readTemperature();

    if (isnan(humidity) || isnan(temperature)) {
        ERA_LOG("DHT", "Failed to read DHT!");
        return;
    }

    // Send the humidity and temperature value to ERa
    ERa.virtualWrite(V0, humidity);
    ERa.virtualWrite(V1, temperature);

    ERA_LOG(ERA_PSTR("Timer"), ERA_PSTR("Uptime: %d"), ERaMillis() / 1000L);
}

/* This function will execute each time from the Terminal Box to your chip */
void terminalCallback() {
    // If you type "Hi"
    // it will response "Hello! Thank you for using ERa."
    if (estr == "Hi") {
        terminal.print("Hello! ");
    }
    terminal.print("Thank you for using ERa.");

    // Send everything into Terminal Box widget
    terminal.flush();
}

void setup() {
    /* Setup debug console */
#if defined(ERA_DEBUG)
    Serial.begin(115200);
#endif

    /* Set board id */
    // ERa.setBoardID("Board_1");

    /* Initialize DHT */
    dht.begin();

    /* Setup the Terminal callback */
    terminal.begin(terminalCallback);

    /* Set timezone +7 */
    syncTime.setTimeZone(7);
    syncTime.begin();

    /* Initializing the ERa library. */
    ERa.begin(ssid, pass);

    /* Setup timer called function every second */
    ERa.addInterval(1000L, timerEvent);
}

void loop() {
    ERa.run();
}
