// https://create.arduino.cc/projecthub/TheGadgetBoy/ds18b20-digital-temperature-sensor-and-arduino-9cc806
/********************************************************************/
// First we include the libraries
#include <OneWire.h>
#include <DallasTemperature.h>

#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <Wire.h>

/********************************************************************/
// Data wire is plugged into pin 2 on the Arduino
#define ONE_WIRE_BUS 3

#define SCREEN_WIDTH 128 // OLED display width, in pixels
#define SCREEN_HEIGHT 32 // OLED display height, in pixels
#define OLED_RESET     9 // Reset pin # (or -1 if sharing Arduino reset pin). Use LED_BUILTIN for NodeMCU applications!!

/********************************************************************/
// Setup a oneWire instance to communicate with any OneWire devices
// (not just Maxim/Dallas temperature ICs)
OneWire oneWire(ONE_WIRE_BUS);

/********************************************************************/
// Pass our oneWire reference to Dallas Temperature.
DallasTemperature sensors(&oneWire);
const uint8_t addressLength = 2;
DeviceAddress addresses[addressLength];

/********************************************************************/
// Setup screen
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);
const uint8_t bufferLength = 50;
char textBuffer[bufferLength];

void setupScreen();
void writeToScreen(const char* text, const uint8_t x = 0, const uint8_t y = 0, const uint8_t textSize = 1);

void setup(void) {  
  // start serial port
  Serial.begin(115200);
  Serial.println("Dallas Temperature IC Control Library Demo");

  setupScreen();
  
  // Start up the library
  sensors.begin();
  char buff[50];
  memset(buff, 50, 0);
  sprintf(buff, "DS18 Devices: %d, total devices: %d", sensors.getDS18Count(), sensors.getDeviceCount());
  Serial.println(buff);

  delay(500);

  for(uint8_t i = 0; i < addressLength; i++) {
    sensors.getAddress(addresses[i], i);
  }
}

void loop(void) {
  // call sensors.requestTemperatures() to issue a global temperature
  // request to all devices on the bus
  
  /********************************************************************/
//  Serial.print(" Requesting temperatures...");
  sensors.requestTemperatures(); // Send the command to get temperature readings
//  Serial.println("DONE");
  
  /********************************************************************/
  float temperature1 = sensors.getTempC(addresses[0]);
  float temperature2 = sensors.getTempC(addresses[1]);

  display.clearDisplay();
  memset(textBuffer, bufferLength, 0);
  sprintf(textBuffer, 
    "Temp1: %d.%02u C",
    (int)temperature1,
    (int)((long)(temperature1 * 100) - (long)temperature1 * 100));
  writeToScreen(textBuffer);
  memset(textBuffer, bufferLength, 0);
  sprintf(textBuffer, 
    "Temp2: %d.%02u C",
    (int)temperature2,
    (int)((long)(temperature2 * 100) - (long)temperature2 * 100));
  writeToScreen(textBuffer, 0, 8);

  display.display();
  
  delay(1000);
}

void setupScreen() {
  if(!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) { // Address 0x3C for 128x32
    Serial.println(F("SSD1306 allocation failed"));
    for(;;); // Don't proceed, loop forever
  }

  display.clearDisplay();

  // Draw a single pixel in white
  display.drawPixel(10, 10, WHITE);
  display.display();
  delay(2000); // Pause for 2 seconds
  
  // Clear the buffer
  display.clearDisplay();
}

void writeToScreen(const char* text, const uint8_t x, const uint8_t y, const uint8_t textSize) {
  display.setTextSize(textSize);      // 1 = Normal 1:1 pixel scale
  display.setTextColor(WHITE);        // Draw white text
  display.setCursor(x, y);
  for(uint8_t i = 0; i < bufferLength; i++) {
    if (text[i] == '\0') {
      break;
    }
    
    display.print(text[i]);
  }
}
