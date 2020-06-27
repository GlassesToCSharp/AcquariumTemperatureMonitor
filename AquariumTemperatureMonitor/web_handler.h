#ifndef WEB_HANDLER_H
#define WEB_HANDLER_H

extern void connectToWifi(void (*onConnecting)(void), void (*onSuccess)(void), void (*onFail)(void));
extern uint8_t httpPost(const String url);

#endif
