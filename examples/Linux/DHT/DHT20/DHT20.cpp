#define NO_GLOBAL_ERA

#include <DHT20.h>
#include <ERaLinux.hpp>

static TwoWire Wire;

static uint8_t dht20Data[7] {0};
static float dht20Humidity {0};
static float dht20Temperature {0};
static uint16_t dht20Address = 0x38;
static const char* i2cDevice = "/dev/i2c-1";

static uint8_t dht20ReadStatus() {
    Wire.requestFrom(dht20Address, 1);
    ERaDelay(1);
    return Wire.read();
}

static bool dht20ResetRegister(uint8_t reg) {
    uint8_t value[3];
    Wire.beginTransmission(i2cDevice, dht20Address);
    Wire.write(reg);
    Wire.write(0x00);
    Wire.write(0x00);
    if (Wire.endTransmission() != 0) {
        return false;
    }
    ERaDelay(5);

    int bytes = Wire.requestFrom(dht20Address, 3);
    for (int i = 0; (i < bytes) && (i < sizeof(value)); i++) {
        value[i] = Wire.read();
    }
    ERaDelay(10);

    Wire.beginTransmission(i2cDevice, dht20Address);
    Wire.write(0xB0 | reg);
    Wire.write(value[1]);
    Wire.write(value[2]);
    if (Wire.endTransmission() != 0) {
        return false;
    }
    ERaDelay(5);

    return true;
}

static uint8_t dht20CRC8(const uint8_t* pData, size_t pDataLen) {
    uint8_t crc = 0xFF;
    while (pDataLen--) {
        crc ^= *pData++;
        for (uint8_t i = 0; i < 8; ++i) {
            if (crc & 0x80) {
                crc <<= 1;
                crc ^= 0x31;
            }
            else {
                crc <<= 1;
            }
        }
    }
    return crc;
}

bool dht20Begin(uint16_t address, const char* device) {
    i2cDevice = device;
    dht20Address = address;
    Wire.beginTransmission(i2cDevice, dht20Address);
    int rv = Wire.endTransmission();
    return (rv == 0);
}

bool dht20IsMeasuring() {
    return (!!(dht20ReadStatus() & 0x80));
}

int dht20RequestData() {
    if ((dht20ReadStatus() & 0x18) != 0x18) {
        dht20ResetRegister(0x1B);
        dht20ResetRegister(0x1C);
        dht20ResetRegister(0x1E);
        ERaDelay(10);
    }

    Wire.beginTransmission(i2cDevice, dht20Address);
    Wire.write(0xAC);
    Wire.write(0x33);
    Wire.write(0x00);
    return Wire.endTransmission();
}

int dht20ReadData() {
    const uint8_t length = 7;
    int bytes = Wire.requestFrom(dht20Address, length);

    if (bytes == 0) {
        return -1;
    }
    if (bytes < length) {
        return -2;
    }

    bool allZero = true;
    for (int i = 0; i < bytes; ++i) {
        dht20Data[i] = Wire.read();
        if (dht20Data[i]) {
            allZero = false;
        }
    }
    if (allZero) {
        return -3;
    }

    return bytes;
}

int dht20Convert() {
    uint32_t raw = dht20Data[1];
    raw <<= 8;
    raw += dht20Data[2];
    raw <<= 4;
    raw += (dht20Data[3] >> 4);
    dht20Humidity = (raw * 9.5367431640625e-5);

    raw = (dht20Data[3] & 0x0F);
    raw <<= 8;
    raw += dht20Data[4];
    raw <<= 8;
    raw += dht20Data[5];
    dht20Temperature = (raw * 1.9073486328125e-4 - 50);

    if (dht20CRC8(dht20Data, 6) != dht20Data[6]) {
        return -1;
    }

    return 0;
}

float dht20GetHumidity() {
    return dht20Humidity;
}

float dht20GetTemperature() {
    return dht20Temperature;
}
