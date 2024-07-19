/*************************************************************
  Download latest ERa library here:
    https://github.com/eoh-jsc/era-lib/releases/latest
    https://www.arduino.cc/reference/en/libraries/era
    https://registry.platformio.org/libraries/eoh-ltd/ERa/installation

    ERa website:                https://e-ra.io
    ERa blog:                   https://iotasia.org
    ERa forum:                  https://forum.eoh.io
    Follow us:                  https://www.fb.com/EoHPlatform
 *************************************************************/

// Enable debug console
// #define ERA_DEBUG
// #define ERA_SERIAL stdout

// Enable Modbus
// #include <ERaSimpleMBLinux.hpp>

// Enable Modbus and Zigbee
#include <ERaLinux.hpp>
#include <ERaOptionsArgs.hpp>
#include <DHT20.h>

static const char* auth;
static const char* boardID;
static const char* host;
static uint16_t port;
static const char* user;
static const char* pass;

// DHT20
const uint16_t dht20Address = 0x38;
const char* i2cDevId = "/dev/i2c-1";

void initDHT() {
    dht20Begin(dht20Address, i2cDevId);
}

void readDHT() {
    if (dht20IsMeasuring()) {
        return;
    }
    if (dht20ReadData() < 0) {
        return;
    }
    if (dht20Convert() == 0) {
        ERa.virtualWrite(V0, dht20GetHumidity());
        ERa.virtualWrite(V1, dht20GetTemperature());
    }
    dht20RequestData();
}

// SRF
const uint8_t trigSRF = 40;
const uint8_t echoSRF = 38;

void initSRF() {
    pinMode(trigSRF, OUTPUT);
    pinMode(echoSRF, INPUT);
}

unsigned long getDistance() {
    unsigned long duration = 0;

    digitalWrite(trigSRF, LOW);
    delayMicroseconds(2);
    digitalWrite(trigSRF, HIGH);
    delayMicroseconds(10);
    digitalWrite(trigSRF, LOW);

    duration = pulseIn(echoSRF, HIGH, 10000UL);
    /* Speed of sound: 343.2 m/s -> 29.1375 us/cm */
    return uint32_t(duration / 2 / 29.1375f);
}

/* This function will run every time ERa is connected */
ERA_CONNECTED() {
    printf("ERa connected!\r\n");
}

/* This function will run every time ERa is disconnected */
ERA_DISCONNECTED() {
    printf("ERa disconnected!\r\n");
}

/* This function print uptime every second */
void timerEvent() {
    readDHT();
    ERa.virtualWrite(V2, getDistance());
    printf("Uptime: %d\r\n", ERaMillis() / 1000L);
}

void setup() {
    initDHT();
    initSRF();
	ERa.setAppLoop(false);
    ERa.setBoardID(boardID);
    ERa.begin(auth, host, port, user, pass);
    ERa.addInterval(1000L, timerEvent);
}

void loop() {
    ERa.run();
}

int main(int argc, char* argv[]) {
    processArgs(argc, argv, auth, boardID,
                host, port, user, pass);

    setup();
    while (1) {
        loop();
    }

    return 0;
}