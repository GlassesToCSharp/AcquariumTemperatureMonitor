#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <DallasTemperature.h>
#include <OneWire.h>
#include <Ticker.h>
#include <Wire.h>

#include "temperature.h"
#include "web_handler.h"

const uint16_t updateFrequency = 1000; // Update every 1s (1000ms).

/********************************************************************/
// Setup a oneWire instance to communicate with any OneWire devices
// (not just Maxim/Dallas temperature ICs)
const uint8_t oneWireBusPin = D4; // Data wire is plugged into D4 (pin 2) on the NodeMCU
OneWire oneWire(oneWireBusPin);

/********************************************************************/
// Pass our oneWire reference to Dallas Temperature.
DallasTemperature sensors(&oneWire);
const uint8_t addressLength = 2;
DeviceAddress addresses[addressLength];

/********************************************************************/
// Timer setup for NodeMCU
Ticker timer;
volatile bool updateDisplayData = false;
volatile bool requestNewData = true;

/********************************************************************/
// Setup screen
const uint8_t screenWidth = 128; // OLED display width, in pixels
const uint8_t screenHeight = 32; // OLED display height, in pixels
const uint8_t oledReset = LED_BUILTIN; // Reset pin # (or -1 if sharing Arduino reset pin). Use LED_BUILTIN for NodeMCU applications!!
Adafruit_SSD1306 display(screenWidth, screenHeight, &Wire, oledReset);

const uint8_t bufferLength = 50;
char textBuffer[bufferLength];
volatile Temperature currentTemperature = {0.0, 0.0, 0};
volatile uint8_t historyTimerCounter = 0;
volatile uint8_t historyCounter = 0;
const uint8_t timeToTakeReading = 3; // Every 60 seconds story a reading.
const uint8_t historyLength = 2;
// New set of data every ([timeToTakeReading] * [historyLength]) seconds
Temperature temperatureHistory[historyLength];
volatile uint32_t timestamp = 1593077684;

// Function declarations
void connectingToWifi();
void connectionSuccess();
void connectionFailed();
void setupScreen();
void writeToScreen(const char* text, const uint8_t x = 0, const uint8_t y = 0);
void displaySensorData(const float sensorData, const uint8_t x = 0, const uint8_t y = 0, bool callDisplay = true);
void showTemperatureLabels();
void timerIsr();

void setup(void) {
  Serial.begin(115200);
  // Setup the OLED display screen.
  setupScreen();

  // Connect to Wifi
  connectToWifi(connectingToWifi, connectionSuccess, connectionFailed);

  // Start up the sensors.
  sensors.begin();

  // For each device, get its address.
  for (uint8_t i = 0; i < addressLength; i++) {
    sensors.getAddress(addresses[i], i);
  }

  // Don't block to wait for conversion.
  sensors.setWaitForConversion(false);

  // Clear the history.
  for (uint8_t i = 0; i < historyLength; i++) {
    clearTemperature(&temperatureHistory[i]);
  }

  // Display the total number of devices and temperature sensors connected.
  display.clearDisplay();
  char buff[bufferLength];
  memset(buff, 0, bufferLength);
  sprintf(buff, "DS18 Devices: %d\nTotal devices: %d", sensors.getDS18Count(), sensors.getDeviceCount());
  writeToScreen(buff);
  display.display();

  delay(2000);

  // Display the titles for the readings.
  showTemperatureLabels();

  timer.attach_ms(updateFrequency, timerIsr);
}

void loop(void) {
  if (updateDisplayData) {
    showTemperatureLabels();
    displaySensorData(currentTemperature.temperature1, 42, 0, false);
    displaySensorData(currentTemperature.temperature2, 42, 8);
    updateDisplayData = false;
  }

  if (historyCounter == historyLength) {
    // When sending data to the server, stop requesting data, as it seems to be
    // causing some timing issues. The requests can be re-enabled after a
    // server response has been received.

    // Disable dta requests.
    requestNewData = false;

    // Show a message saying data is being uploaded.
    // TODO: replace this with an icon.
    display.clearDisplay();
    memset(textBuffer, 0, bufferLength);
    sprintf(textBuffer, "Uploading data...");
    writeToScreen(textBuffer);
    display.display();
    
    // Send data to server.
    uint16_t responseCode = uploadData(temperatureHistory, historyLength);
    if (responseCode < 200 || responseCode > 299) {
      sprintf(textBuffer, "Failed to send\ndata: %d", responseCode);
      writeToScreen(textBuffer);
      display.display();
    }

    // Re-enable data requests.
    requestNewData = true;

    // Clear history.
    for (uint8_t i = 0; i < historyLength; i++) {
      Serial.print(temperatureHistory[i].temperature1);
      Serial.print(" | ");
      Serial.print(temperatureHistory[i].temperature2);
      Serial.print(" | ");
      Serial.println(temperatureHistory[i].time);
      clearTemperature(&temperatureHistory[i]);
    }
    Serial.println();
    historyCounter = 0;
  }
}


void connectingToWifi() {
  static uint8_t loadingIndex = 0;
  const uint8_t loadingCharactersCapacity = 4;
  const uint8_t loadingCharacters[4] = {'|', '/', '-', '\\' };
  display.clearDisplay();
  memset(textBuffer, 0, bufferLength);
  sprintf(textBuffer, "Connecting to\nWiFi... %c", loadingCharacters[loadingIndex]);
  writeToScreen(textBuffer);
  display.display();

  loadingIndex++;
  if (loadingIndex == loadingCharactersCapacity) {
    loadingIndex = 0;
  }
}

void connectionSuccess() {
  display.clearDisplay();
  memset(textBuffer, 0, bufferLength);
  sprintf(textBuffer, "Connected to Wifi\nsuccessfully!");
  writeToScreen(textBuffer);
  display.display();

  // Wait here to show the message on the display.
  delay(2000);
}

void connectionFailed() {
  display.clearDisplay();
  memset(textBuffer, 0, bufferLength);
  sprintf(textBuffer, "Failed to connect.\nCheck WiFi status and\nrestart the device.");
  writeToScreen(textBuffer);
  display.display();
}

void setupScreen() {
  if (!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) { // Address 0x3C for 128x32
    const uint8_t ledPin = 13;
    pinMode(ledPin, OUTPUT);
    for (;;) {
      // Don't proceed, loop forever
      digitalWrite(ledPin, HIGH);
      delay(500);
      digitalWrite(ledPin, LOW);
      delay(500);
    }
  }

  display.clearDisplay();

  // Draw a single pixel in white
  display.drawPixel(10, 10, WHITE);
  display.display();
  delay(1000); // Pause for 2 seconds

  // Clear the buffer
  display.clearDisplay();
}

void displaySensorData(const float sensorData, const uint8_t x, const uint8_t y, bool callDisplay) {
  memset(textBuffer, 0, bufferLength);
  if ((int)sensorData != DEVICE_DISCONNECTED_C) {
    sprintf(textBuffer,
            "%d.%02u C  ", // Trailing space to overwrite any text left over from a disconnected sensor
            (int)sensorData,
            (int)((long)(sensorData * 100) - (long)sensorData * 100));
  } else {
    sprintf(textBuffer, "No sensor");
  }

  // Display the text buffer
  writeToScreen(textBuffer, x, y);
  if (callDisplay) {
    display.display();
  }
}

void showTemperatureLabels() {
  // Display the titles for the readings.
  display.clearDisplay();
  memset(textBuffer, 0, bufferLength);
  sprintf(textBuffer, "Temp1:\nTemp2:");
  writeToScreen(textBuffer);
  display.display();
}

void writeToScreen(const char* text, const uint8_t x, const uint8_t y) {
  const uint8_t textSize = 1;
  display.setTextSize(textSize);      // 1 = Normal 1:1 pixel scale
  display.setTextColor(WHITE, BLACK);        // Draw white text on black background
  display.setCursor(x, y);
  for (uint8_t i = 0; i < bufferLength; i++) {
    if (text[i] == '\0') {
      break;
    }

    display.print(text[i]);
  }
}

// Interrupt Service Routine for Timer.
void timerIsr() {
  static bool temperatureRequested = false;

  // Update the local time.
  timestamp++;

  // If the main program decides that the timer should not request data (for example,
  // to stop interrupting uploading of data), the timer can just return. This must be
  // done after updating the timestamp so as to keep track of time.
  if (!requestNewData) {
    return;
  }
  
  // Send the command to all sensors on the bus to get temperature readings.
  if (!temperatureRequested) {
    sensors.requestTemperatures();
    temperatureRequested = true;
  }

  // The requested temperatures will take some time to convert. While converting,
  // DO NOT BLOCK the MCU from doing critical tasks (eg maintaining a connection.
  // NOTE: The DallasTemperature library uses the `yield()` function, which should
  // prevent any blocking on the NodeMCU, but this seems to cause crashes. Checking
  // for a complete conversion every time the ISR is triggered seems to be a good
  // workaround.
  bool conversionComplete = sensors.isConversionComplete();
  Serial.print("Temperature requested: ");
  Serial.print(temperatureRequested);
  Serial.print(", conversion complete: ");
  Serial.println(conversionComplete);
  if (temperatureRequested && conversionComplete) {
    float tempSensor1 = sensors.getTempC(addresses[0]); // Gets the values of the temperature
    float tempSensor2 = sensors.getTempC(addresses[1]); // Gets the values of the temperature

    // Present readings on the OLED display.
    if ((currentTemperature.temperature1 != tempSensor1) || (currentTemperature.temperature2 != tempSensor2)) {
      updateDisplayData = true;
    }

    setNewTemperature(&currentTemperature, &tempSensor1, &tempSensor2, &timestamp);

    temperatureRequested = false;
  }

  historyTimerCounter++;
  if (historyTimerCounter == timeToTakeReading) {
    historyTimerCounter = 0;
    setNewTemperature(&temperatureHistory[historyCounter], &currentTemperature);
    historyCounter++;
  }
}
