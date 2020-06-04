#include <OneWire.h>
#include <DallasTemperature.h>

#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <Wire.h>

const uint16_t updateFrequency = 1000; // Update every 1s (1000ms).

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
float temperature1 = 0.0;
float temperature2 = 0.0;
volatile float currentTemperature1 = 0.0;
volatile float currentTemperature2 = 0.0;

// Function declarations
void setupScreen();
void writeToScreen(const char* text, const uint8_t x = 0, const uint8_t y = 0);
void displaySensorData(const float sensorData, const uint8_t x = 0, const uint8_t y = 0);
void setupInterrupt();

void setup(void) {
  // Setup the OLED display screen.
  setupScreen();

  // Start up the sensors.
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
  for (uint8_t i = 0; i < addressLength; i++) {
    sensors.getAddress(addresses[i], i);
  }

  // Display the titles for the readings. These will never change, so this code can stay here.
  display.clearDisplay();
  memset(textBuffer, bufferLength, 0);
  sprintf(textBuffer, "Temp1:\nTemp2:");
  writeToScreen(textBuffer);
  display.display();

  setupInterrupt();
}

void loop(void) {
}

void setupScreen() {
  if (!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) { // Address 0x3C for 128x32
    pinMode(13, OUTPUT);
    for (;;) {
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
  for (uint8_t i = 0; i < bufferLength; i++) {
    if (text[i] == '\0') {
      break;
    }

    display.print(text[i]);
  }
}

void setupInterrupt() {
  cli(); // Stop any other interrupts.

  // We want an interrupt frequency of 1Hz (1 second). The Nano has 3 timers:
  // - Timer 0 (8-bit)
  // - Timer 1 (16-bit)
  // - Timer 2 (8-bit)
  // The correct timer to use is determined by the Compare Match Register (OCRxA - x
  // is timer number). The CMR value is determined with the following equation:
  // CMR = [ ClockFrequency / (Prescaler * DesiredInterruptFrequency) ] - 1
  // The clock runs at 16MHz, and the largest prescaler the interrupts can take is
  // 1024. The equation then becomes:
  // CMR = [ 16,000,000 / (1024 * 1) ] - 1
  // CMR = 15,624
  // Since 15,624 exceeds the 8-bit range of timers 0 and 2, but is within the range
  // of timer 1, Timer 1 is the correct timer to use.

  // Set up Timer 1 for 1Hz interrupt.

  // Reset Timer 1 registers
  TCCR1A = 0; // Set Timer/Counter Control Register A to Normal Operation throughout.
  TCCR1B = 0; // Clear the Timer/Counter Control Register B.
  TCNT1  = 0; // Clear the Timer/Counter (initialise to 0).

  // Set the Compare Match Register A for 1Hz triggers.
  OCR1A = 15624; // From equation above.

  // Turn on Clear Timer on Compare Match (CTC) mode in the Timer/Counter Control
  // Register A.
  TCCR1B |= (1 << WGM12); // Raise the WGM12 bit.

  // Set the Clock Select bits (CS12:10) to the 1024 prescaler value.
  TCCR1B |= ((1 << CS12) | (1 << CS10));

  // Enable the timer interrupt in the Timer/Counter Interrupt Mask Register.
  TIMSK1 |= (1 << OCIE1A); // Enable the timer on the CMR for Register A.

  sei(); // Allow interrupts
}

// Interrupt Service Routine for Timer 1.
// Timer 1 Compare Match A.
// On Compare Match A == CMR A.
ISR(TIMER1_COMPA_vect) {
  // Send the command to all sensors on the bus to get temperature readings.
  sensors.requestTemperatures();

  // Once complete, get the actual temperatures in Celsius.
  temperature1 = sensors.getTempC(addresses[0]);
  temperature2 = sensors.getTempC(addresses[1]);

  // Present readings on the OLED display.
  if (currentTemperature1 != temperature1) {
    currentTemperature1 = temperature1;
    displaySensorData(temperature1, 42, 0);
  }

  if (currentTemperature2 != temperature2) {
    currentTemperature2 = temperature2;
    displaySensorData(temperature2, 42, 8);
  }
}
