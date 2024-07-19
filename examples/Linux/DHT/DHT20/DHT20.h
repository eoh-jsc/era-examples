#ifndef INC_DHT20_H_
#define INC_DHT20_H_

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

bool dht20Begin(uint16_t address, const char* device);
bool dht20IsMeasuring();
int dht20RequestData();
int dht20ReadData();
int dht20Convert();
float dht20GetHumidity();
float dht20GetTemperature();

#ifdef __cplusplus
}
#endif

#endif /* INC_DHT20_H_ */
