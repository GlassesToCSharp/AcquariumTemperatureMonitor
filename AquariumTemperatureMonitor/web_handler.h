#ifndef WEB_HANDLER_H
#define WEB_HANDLER_H

#include "temperature.h"

extern void connectToWifi(void (*onConnecting)(void), void (*onSuccess)(void), void (*onFail)(void));
extern uint16_t uploadData(const Temperature * data, const uint8_t dataLength);
extern void updateTime(volatile uint32_t * timestamp);

#endif
