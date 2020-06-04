#include <OneWire.h>
#include <DallasTemperature.h>

#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <Wire.h>

/********************************************************************/
// Setup a oneWire instance to communicate with any OneWire devices
// (not just Maxim/Dallas temperature ICs)
const uint8_t oneWireBusPin = 3; // Data wire is plugged into pin 3 on the Arduino
OneWire oneWire(oneWireBusPin);

/********************************************************************/
// Pass our oneWire reference to Dallas Temperature.
DallasTemperature sensors(&oneWire);
const uint8_t addressLength = 2;
DeviceAddress addresses[addressLength];

/********************************************************************/
// Setup screen
const uint8_t screenWidth = 128; // OLED display width, in pixels
const uint8_t screenHeight = 32; // OLED display height, in pixels
const uint8_t oledReset = 9; // Reset pin # (or -1 if sharing Arduino reset pin). Use LED_BUILTIN for NodeMCU applications!!
Adafruit_SSD1306 display(screenWidth, screenHeight, &Wire, oledReset);
const uint8_t bufferLength = 50;
char textBuffer[bufferLength];

// Function declarations
void setupScreen();
void writeToScreen(const char* text, const uint8_t x = 0, const uint8_t y = 0);
void displaySensorData(const float sensorData, const uint8_t x = 0, const uint8_t y = 0);

void setup(void) {  
  setupScreen();
  
  // Start up the library
  sensors.begin();

  // Display the total number of devices and temperature sensors connected.
  display.clearDisplay();
  char buff[bufferLength];
  memset(buff, bufferLength, 0);
  sprintf(buff, "DS18 Devices: %d\nTotal devices: %d", sensors.getDS18Count(), sensors.getDeviceCount());
  writeToScreen(buff);
  display.display();

  delay(2000);

  // For each device, get its address.
  for(uint8_t i = 0; i < addressLength; i++) {
    sensors.getAddress(addresses[i], i);
  }

  // Display the titles for the readings. These will never change, so this code can stay here.
  display.clearDisplay();
  memset(textBuffer, bufferLength, 0);
  sprintf(textBuffer, "Temp1:\nTemp2:");
  writeToScreen(textBuffer);
  display.display();
}

void loop(void) {
  // Send the command to all sensors on the bus to get temperature readings.
  sensors.requestTemperatures(); 
  
  // Once complete, get the actual temperatures in Celsius.
  float temperature1 = sensors.getTempC(addresses[0]);
  float temperature2 = sensors.getTempC(addresses[1]);

  // Present on the OLED display.
  displaySensorData(temperature1, 42, 0);
  displaySensorData(temperature2, 42, 8);
  
  delay(1000);
}

void setupScreen() {
  if(!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) { // Address 0x3C for 128x32
    pinMode(13, OUTPUT);
    for(;;) {
      // Don't proceed, loop forever
      digitalWrite(13, HIGH);
      delay(500);
      digitalWrite(13, LOW);
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

void displaySensorData(const float sensorData, const uint8_t x, const uint8_t y) {
  memset(textBuffer, bufferLength, 0);
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
  display.display();
}

void writeToScreen(const char* text, const uint8_t x, const uint8_t y) {
  const uint8_t textSize = 1;
  display.setTextSize(textSize);      // 1 = Normal 1:1 pixel scale
  display.setTextColor(WHITE, BLACK);        // Draw white text on black background
  display.setCursor(x, y);
  for(uint8_t i = 0; i < bufferLength; i++) {
    if (text[i] == '\0') {
      break;
    }
    
    display.print(text[i]);
  }
}
