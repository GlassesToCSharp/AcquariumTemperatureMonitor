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
// Use DynamicJsonDocument for document size larger than 1KB.
DynamicJsonDocument postJsonDoc(serializedJsonCapacity);
StaticJsonDocument<serializedTemperatureJsonCapacity> temperatureJsonDoc;
// 42 bytes per object (subject to change, and assumes 5 chars per floating
// point value), 1 byte for the commas to separate objects in the list, 2
// bytes for the starting and ending brackets ( [ ] ) to denote a list object.
const uint16_t postJsonCapacity = (42 + 1 + 2) * numberOfDataPoints;
char postJsonString[postJsonCapacity];

const char dataKey[] = "data";
const char temperature1Key[] = "t1";
const char temperature2Key[] = "t2";
const char timeKey[] = "time";
const char postDataAddress[] PROGMEM = "/water/data";
const String serverAddress = "http://192.168.1.77:3000";

void connectToWifi(void (*onConnecting)(void), void (*onSuccess)(void), void (*onFail)(void)) {
  Serial.println("Connecting to Wifi");
  WiFi.begin(ssid, password);
  Serial.println("Connecting process started");
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
    char buff[200];
    sprintf(buff, "Serializing: %f, %f, %d", dataPoint->temperature1, dataPoint->temperature2, dataPoint->time);
    Serial.println(buff);
    if (dataPoint->time == 0) {
      // On reset, the time variable will be 0. If so, there is no more data
      // in the array.
      break;
    }

    JsonObject jsonObject = temperatureJsonDoc.to<JsonObject>();
    jsonObject[temperature1Key] = dataPoint->temperature1;
    jsonObject[temperature2Key] = dataPoint->temperature2;
    jsonObject[timeKey] = dataPoint->time;

    memset(buff, 0, 200);
    sprintf(buff, "Serialized: %f, %f, %d", (float)temperatureJsonDoc[temperature1Key], (float)temperatureJsonDoc[temperature2Key], (uint32_t)temperatureJsonDoc[timeKey]);
    Serial.println(buff);

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

//  return 200;

  // Expecting a 204, so only return the status code.
  return http.POST(postJsonString);
}

uint16_t uploadData(const Temperature * data, const uint8_t dataLength) {
  generateJson(data, dataLength);
  char buff[200];
  String url = serverAddress + postDataAddress;
  sprintf(buff, "Address: %s\nData: %s", url.c_str(), postJsonString);
  Serial.println(buff);
  return httpPost(serverAddress + postDataAddress);
}
