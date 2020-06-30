#include <Arduino.h>
#include <ArduinoJson.h>
#include <ESP8266WiFi.h>
#include <ESP8266HTTPClient.h>

#include "credentials.h"
#include "temperature.h"

HTTPClient http;
WiFiClient client;

// Expected JSON object format:
//    {
//        "t1": 25.01,
//        "t2": 25.02,
//        "time": 1351824120
//    }
const uint8_t numberOfDataPoints = 100;
const uint8_t sizePerObject = 64; // 32 for AVR systems, 64 for all else.
const size_t serializedTemperatureJsonCapacity = JSON_OBJECT_SIZE(3) + 11;
const size_t serializedJsonCapacity =
  JSON_ARRAY_SIZE(numberOfDataPoints) +
  (numberOfDataPoints * JSON_OBJECT_SIZE(3)) +
  (numberOfDataPoints * sizePerObject);
const size_t serializedTimeJsonCapacity = JSON_OBJECT_SIZE(6) + 35 + 131; // As per the assistant
// Use DynamicJsonDocument for document size larger than 1KB.
DynamicJsonDocument postJsonDoc(serializedJsonCapacity);
StaticJsonDocument<serializedTemperatureJsonCapacity> temperatureJsonDoc;
StaticJsonDocument<serializedTimeJsonCapacity> timeJsonDoc;
// 42 bytes per object (subject to change, and assumes 5 chars per floating
// point value), 1 byte for the commas to separate objects in the list, 2
// bytes for the starting and ending brackets ( [ ] ) to denote a list object.
const uint16_t postJsonCapacity = (42 + 1 + 2) * numberOfDataPoints;
char postJsonString[postJsonCapacity];

const char dataKey[] = "data";
const char temperature1Key[] = "t1";
const char temperature2Key[] = "t2";
const char timeKey[] = "time";
const char unixTimeKey[] = "unix_time";
const char postDataAddress[] PROGMEM = "/water/data";
const char getTimeAddress[] PROGMEM = "/currentTime?format=UNIX_S";
const String serverAddress = "http://192.168.1.77:3000";

void connectToWifi(void (*onConnecting)(void), void (*onSuccess)(void), void (*onFail)(void)) {
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    if (onConnecting != NULL) {
      (*onConnecting)();
    }

    if (WiFi.status() == WL_CONNECT_FAILED) {
      if (onFail != NULL) {
        (*onFail)();
      }
      while (1); // stay here
    }
  }

  if (onSuccess != NULL) {
    (*onSuccess)();
  }
}

void generateJson(const Temperature * data, const uint8_t dataLength) {
  if (data == NULL) {
    // Appropriate error?
    return;
  }

  postJsonDoc.clear();

  JsonArray jsonList = postJsonDoc.createNestedArray(dataKey);

  for (uint8_t i = 0; i < dataLength; i++) {
    const Temperature * dataPoint = data + i;
    if (dataPoint->time == 0) {
      // On reset, the time variable will be 0. If so, there is no more data
      // in the array.
      break;
    }

    JsonObject jsonObject = temperatureJsonDoc.to<JsonObject>();
    jsonObject[temperature1Key] = dataPoint->temperature1;
    jsonObject[temperature2Key] = dataPoint->temperature2;
    jsonObject[timeKey] = dataPoint->time;

    jsonList.add(jsonObject);
  }

  memset(postJsonString, 0, postJsonCapacity);
  serializeJson(postJsonDoc, postJsonString, postJsonCapacity);
}

// Eurgh, String...
uint16_t httpPost(const String url) {
  // End the current connection, even if never started.
  http.end();

  // Begin a new connection.
  http.begin(client, url);
  http.addHeader("Content-Type", "application/json"); //Specify content-type header

  // Expecting a 204, so only return the status code.
  return http.POST(postJsonString);
}

// Eurgh, String...
String httpGet(const String url) {
  // End the current connection, even if never started.
  http.end();

  // Begin a new connection.
  http.begin(client, url);
  int httpResponseCode = http.GET();
  if (httpResponseCode > 0) {
    // The request was successful, but it may be a 404 or sumat...
    return http.getString();
  } else {
    // Likely the endpoint could not be reached.
    return "";
  }
}

uint16_t uploadData(const Temperature * data, const uint8_t dataLength) {
  generateJson(data, dataLength);
  char buff[200];
  sprintf(buff, "%s%s", serverAddress.c_str(), postDataAddress);
  return httpPost(String(buff));
}

void updateTime(volatile uint32_t * timestamp) {
  String json = httpGet(serverAddress + getTimeAddress);
  if (json == "") {
    return;
  }

  timeJsonDoc.clear();
  DeserializationError deserializationError = deserializeJson(timeJsonDoc, json);
  if (!deserializationError) {
    *timestamp = timeJsonDoc[unixTimeKey];
  }
}
