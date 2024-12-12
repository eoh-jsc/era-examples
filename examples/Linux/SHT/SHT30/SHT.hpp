#ifndef INC_SHT_HPP_
#define INC_SHT_HPP_

#if defined(ARDUINO)
    #include <Arduino.h>
    #include <Wire.h>
#elif defined(LINUX)
    #include <Utility/ERaI2CWiringPi.hpp>
#endif
#include <ERa/ERaProtocol.hpp>

#ifndef SHT_DEFAULT_ADDRESS
    #define SHT_DEFAULT_ADDRESS         0x44
#endif

#define SHT_STATUS_ALERT_PENDING        (1 << 15)
#define SHT_STATUS_HEATER_ON            (1 << 13)
#define SHT_STATUS_HUM_TRACK_ALERT      (1 << 11)
#define SHT_STATUS_TEMP_TRACK_ALERT     (1 << 10)
#define SHT_STATUS_SYSTEM_RESET         (1 << 4)
#define SHT_STATUS_COMMAND_STATUS       (1 << 1)
#define SHT_STATUS_WRITE_CRC_STATUS     (1 << 0)

#define SHT_OK                          0x00
#define SHT_ERR_WRITECMD                0x81
#define SHT_ERR_READBYTES               0x82
#define SHT_ERR_HEATER_OFF              0x83
#define SHT_ERR_NOT_CONNECT             0x84
#define SHT_ERR_CRC_TEMP                0x85
#define SHT_ERR_CRC_HUM                 0x86
#define SHT_ERR_CRC_STATUS              0x87
#define SHT_ERR_HEATER_COOLDOWN         0x88
#define SHT_ERR_HEATER_ON               0x89
#define SHT_ERR_SERIAL                  0x8A

#define SHT_READ_STATUS                 0xF32D
#define SHT_CLEAR_STATUS                0x3041

#define SHT_SOFT_RESET                  0x30A2
#define SHT_HARD_RESET                  0x0006

#define SHT_MEASUREMENT_FAST            0x2416
#define SHT_MEASUREMENT_SLOW            0x2400

#define SHT_HEAT_ON                     0x306D
#define SHT_HEAT_OFF                    0x3066
#define SHT_HEATER_TIMEOUT              180000UL

#define SHT_GET_SERIAL                  0x3682

namespace eras {

    class SHT
    {
    public:
        SHT()
            : mWire(NULL)
            , mDeviceAddress(0)
            , mHeatTimeout(0)
            , mLastRead(0)
            , mLastRequest(0)
            , mHeaterStart(0)
            , mHeaterStop(0)
            , mHeaterOn(false)
            , mType(0)
            , mRawHumidity(0)
            , mRawTemperature(0)
            , mTemperatureOffset(0.0f)
            , mHumidityOffset(0.0f)
            , mError(SHT_OK)
        {}

        bool begin(TwoWire* wire, uint8_t deviceAddress) {
            if ((deviceAddress != 0x44) && (deviceAddress != 0x45)) {
                return false;
            }

            this->mWire = wire;
            this->mDeviceAddress = deviceAddress;
            return this->reset();
        }

        uint8_t getAddress() {
            return this->mDeviceAddress;
        }

        uint8_t getType() {
            return this->mType;
        }

        bool read(bool fast = true) {
            if (!this->requestData(fast)) {
                return false;
            }

            while (!this->dataReady(fast)) {}

            return this->readData(fast);
        }

        bool requestData(bool fast = true) {
            if (!this->writeCmd(fast ? SHT_MEASUREMENT_FAST : SHT_MEASUREMENT_SLOW)) {
                return false;
            }

            this->mLastRequest = ERaMillis();
            return true;
        }

        bool dataReady(bool fast = true) {
            return ((ERaMillis() - this->mLastRequest) > (fast ? 4 : 15));
        }

        bool readData(bool fast = true) {
            uint8_t buffer[6];

            if (!this->readBytes(buffer, 6)) {
                return false;
            }

            if (!fast) {
                if (buffer[2] != this->crc8(buffer, 2)) {
                    this->mError = SHT_ERR_CRC_TEMP;
                    return false;
                }
                if (buffer[5] != this->crc8(buffer + 3, 2)) {
                    this->mError = SHT_ERR_CRC_HUM;
                    return false;
                }
            }

            this->mRawTemperature = ((buffer[0] << 8) + buffer[1]);
            this->mRawHumidity = ((buffer[3] << 8) + buffer[4]);

            this->mLastRead = ERaMillis();

            this->mError = SHT_OK;
            return true;
        }

        uint32_t lastRequest() {
            return this->mLastRequest;
        }

        bool isConnected() {
            this->mWire->beginTransmission(this->mDeviceAddress);
            int rv = this->mWire->endTransmission();
            if (rv != 0) {
                this->mError = SHT_ERR_NOT_CONNECT;
                return false;
            }
            this->mError = SHT_OK;
            return true;
        }

        uint16_t readStatus() {
            uint8_t status[3] = { 0, 0, 0 };

            if (!this->writeCmd(SHT_READ_STATUS)) {
                return 0xFFFF;
            }
            if (!this->readBytes(status, 3)) {
                return 0xFFFF;
            }

            if (status[2] != this->crc8(status, 2)) {
                this->mError = SHT_ERR_CRC_STATUS;
                return 0xFFFF;
            }
            return ((uint16_t)((status[0] << 8) + status[1]));
        }

        uint32_t lastRead() {
            return this->mLastRead;
        }

        bool reset(bool hard = false) {
            if (!this->writeCmd(hard ? SHT_HARD_RESET : SHT_SOFT_RESET)) {
                return false;
            }

            ERaDelay(1);
            return true;
        }

        int getError() {
            int rv = this->mError;
            this->mError = SHT_OK;
            return rv;
        }

        void setHeatTimeout(uint8_t seconds) {
            this->mHeatTimeout = seconds;
            if (this->mHeatTimeout > 180) {
                this->mHeatTimeout = 180;
            }
        }

        uint8_t getHeatTimeout() {
            return this->mHeatTimeout;
        }

        bool heatOn() {
            if (this->isHeaterOn()) {
                return true;
            }

            if ((this->mHeaterStop > 0) && ((ERaMillis() - this->mHeaterStop) < SHT_HEATER_TIMEOUT)) {
                this->mError = SHT_ERR_HEATER_COOLDOWN;
                return false;
            }

            if (this->writeCmd(SHT_HEAT_ON) == false) {
                this->mError = SHT_ERR_HEATER_ON;
                return false;
            }
            this->mHeaterStart = ERaMillis();
            this->mHeaterOn = true;
            return true;
        }

        bool heatOff() {
            if (!this->writeCmd(SHT_HEAT_OFF)) {
                this->mError = SHT_ERR_HEATER_OFF;
                return false;
            }

            this->mHeaterStop = ERaMillis();
            this->mHeaterOn = false;
            return true;
        }

        bool isHeaterOn() {
            if (!this->mHeaterOn) {
                return false;
            }

            if ((ERaMillis() - this->mHeaterStart) < (this->mHeatTimeout * 1000UL)) {
                return true;
            }
            this->heatOff();
            return false;
        }

        float getHumidity() {
            float hum = (this->mRawHumidity * (100.0 / 65535));
            if (this->mHumidityOffset != 0) {
                hum += this->mHumidityOffset;
            }
            return hum;
        }

        float getTemperature() {
            float temp = ((this->mRawTemperature * (175.0 / 65535)) - 45);
            if (this->mTemperatureOffset != 0) {
                temp += this->mTemperatureOffset;
            }
            return temp;
        }

        float getFahrenheit() {
            float temp = ((this->mRawTemperature * (63.0 / 13107.0)) - 49);
            if (this->mTemperatureOffset != 0) {
                temp += (this->mTemperatureOffset * 1.8);
            }
            return temp;
        }

        uint16_t getRawHumidity() {
            return this->mRawHumidity;
        }

        uint16_t getRawTemperature() {
            return this->mRawTemperature;
        }

        void setTemperatureOffset(float offset = 0.0f) {
            this->mTemperatureOffset = offset;
        }

        float getTemperatureOffset() {
            return this->mTemperatureOffset;
        }

        void setHumidityOffset(float offset = 0.0f) {
            this->mHumidityOffset = offset;
        }

        float getHumidityOffset() {
            return this->mHumidityOffset;
        }

    protected:
        uint8_t crc8(const uint8_t *data, uint8_t len) {
            uint8_t crc(0xFF);
            const uint8_t POLY(0x31);

            for (uint8_t j = len; j; --j) {
                crc ^= *data++;

                for (uint8_t i = 8; i; --i) {
                    crc = ((crc & 0x80) ? ((crc << 1) ^ POLY) : (crc << 1));
                }
            }
            return crc;
        }

        bool writeCmd(uint16_t cmd) {
            this->mWire->beginTransmission(this->mDeviceAddress);
            this->mWire->write(cmd >> 8 );
            this->mWire->write(cmd & 0xFF);
            if (this->mWire->endTransmission() != 0) {
                this->mError = SHT_ERR_WRITECMD;
                return false;
            }
            this->mError = SHT_OK;
            return true;
        }

        bool readBytes(uint8_t* buf, uint8_t len) {
            int rv = this->mWire->requestFrom(this->mDeviceAddress, len);
            if (rv != len) {
                this->mError = SHT_ERR_READBYTES;
                return false;
            }
            for (uint8_t i = 0; i < len; ++i) {
                buf[i] = this->mWire->read();
            }
            this->mError = SHT_OK;
            return true;
        }

        TwoWire* mWire;

        uint8_t mDeviceAddress;
        uint8_t mHeatTimeout;
        uint32_t mLastRead;
        uint32_t mLastRequest;
        uint32_t mHeaterStart;
        uint32_t mHeaterStop;
        bool mHeaterOn;
        uint8_t mType;

        uint16_t mRawHumidity;
        uint16_t mRawTemperature;

        float mTemperatureOffset;
        float mHumidityOffset;

        uint8_t mError;
    };

    class SHT30
        : public SHT
    {
    public:
        SHT30()
        {
            this->mType = 30;
        }
    };

}

using namespace eras;

#endif /* INC_SHT_HPP_ */
