#include <OneWire.h>
#include <DallasTemperature.h>

#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include "temperature.h"
#include <Wire.h>

// The external interrupt is a future implementation, which does not need to be included
// at this point (there are no suitable features for it yet). The idea is to have a button
// to scroll past different screens (Celsius, Fahrenheit, graph, etc). This is not a
// priority, but it is a work in progress.
//#define INCLUDE_INTERRUPTS

const uint16_t updateFrequency = 1000; // Update every 1s (1000ms).
#ifdef INCLUDE_INTERRUPTS
const uint8_t debounceDelay = 50; // Debounce delay in ms.
#endif

/********************************************************************/
// Setup a oneWire instance to communicate with any OneWire devices
// (not just Maxim/Dallas temperature ICs)
const uint8_t oneWireBusPin = 4; // Data wire is plugged into pin 4 on the Arduino
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
volatile Temperature currentTemperature = {0.0, 0.0};
volatile uint8_t historyTimerCounter = 0;
volatile uint8_t historyCounter = 0;
const uint8_t timeToTakeReading = 60; // Every 60 seconds story a reading.
const uint8_t historyLength = 100;
// New set of data every ([timeToTakeReading] * [historyLength]) seconds
Temperature temperatureHistory[historyLength];

// Function declarations
void setupScreen();
void writeToScreen(const char* text, const uint8_t x = 0, const uint8_t y = 0);
void displaySensorData(const float sensorData, const uint8_t x = 0, const uint8_t y = 0);
#ifdef INCLUDE_INTERRUPTS
void setupExternalInterrupt();
#endif
void setupTimerInterrupts();

void setup(void) {
  // Setup the OLED display screen.
  setupScreen();

  // Start up the sensors.
  sensors.begin();

  // Clear the history.
  for(uint8_t i = 0; i < historyLength; i++) {
    clearTemperature(&temperatureHistory[i]);
  }
  
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

  cli(); // Stop any other interrupts.
  #ifdef INCLUDE_INTERRUPTS
  setupExternalInterrupt();
  #endif
  setupTimerInterrupts();
  sei(); // Enable interrupts
}

void loop(void) {
  if (historyCounter == historyLength) {
    for(uint8_t i = 0; i < historyLength; i++) {
      // TODO: Send to server
//      Serial.print(temperatureHistory[i].temperature1);
//      Serial.print(" | ");
//      Serial.println(temperatureHistory[i].temperature2);
      clearTemperature(&temperatureHistory[i]);
    }
//    Serial.println();
    historyCounter = 0;
  }
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

void setupTimerInterrupts() {
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
  // Register B.
  TCCR1B |= (1 << WGM12); // Raise the WGM12 bit.

  // Set the Clock Select bits (CS12:10) to the 1024 prescaler value.
  TCCR1B |= ((1 << CS12) | (1 << CS10));

  // Enable the timer interrupt in the Timer/Counter Interrupt Mask Register.
  TIMSK1 |= (1 << OCIE1A); // Enable the timer on the CMR for Register A.

  #ifdef INCLUDE_INTERRUPTS
  //------------------
  // Set up Timer 2 for 1kHz interrupt.

  // Reset Timer 1 registers
  TCCR2A = 0; // Set Timer/Counter Control Register A to Normal Operation throughout.
  TCCR2B = 0; // Clear the Timer/Counter Control Register B.
  TCNT2  = 0; // Clear the Timer/Counter (initialise to 0).

  // Set the Compare Match Register A for 1kHz triggers.
  OCR2A = 124; // From equation above.

  // Turn on Clear Timer on Compare Match (CTC) mode in the Timer/Counter Control
  // Register A.
  TCCR2A |= (1 << WGM21); // Raise the WGM21 bit.

  // Set the Clock Select bits (CS22:20) to the 128 prescaler value.
  TCCR2B |= ((1 << CS22) | (1 << CS20));

  // Enable the timer interrupt in the Timer/Counter Interrupt Mask Register.
  //  TIMSK2 |= (1 << OCIE2A); // Enable the timer on the CMR for Register A.
  #endif
}

// Interrupt Service Routine for Timer 1.
// Timer 1 Compare Match A.
// On Compare Match A == CMR A.
ISR(TIMER1_COMPA_vect) {
  // Send the command to all sensors on the bus to get temperature readings.
  sensors.requestTemperatures();

  // Once complete, get the actual temperatures in Celsius.
  float temperature1 = sensors.getTempC(addresses[0]);
  float temperature2 = sensors.getTempC(addresses[1]);

  // Present readings on the OLED display.
  if (currentTemperature.temperature1 != temperature1) {
    displaySensorData(temperature1, 42, 0);
  }

  if (currentTemperature.temperature2 != temperature2) {
    displaySensorData(temperature2, 42, 8);
  }

  setTemperatures(&currentTemperature, &temperature1, &temperature2);

  historyTimerCounter++;
  if (historyTimerCounter == timeToTakeReading) {
    historyTimerCounter = 0;
    setTemperatures(&temperatureHistory[historyCounter], &temperature1, &temperature2);
    historyCounter++;
  }
}

#ifdef INCLUDE_INTERRUPTS
// Interrupt Service Routine for Timer 2.
// Timer 2 Compare Match A.
// On Compare Match A == CMR A.
ISR(TIMER2_COMPA_vect) {
  static uint16_t counter = 0;
  counter++;

  if (counter > debounceDelay) {
    counter = 0;
    TIMSK2 &= ~(1 << OCIE2A); // Disable the debounce timer Timer2. No need to keep it running.
    EIMSK |= (1 << INT0); // Enable the external interrupt INT0.
  }
}

void setupExternalInterrupt() {
  // We want an external interrupt occurring so that we can change the display. The
  // Nano (ATMega328P) has multiple external interrupt sources:
  // - External Interrupt Request 0  (pin D2)          (INT0_vect)
  // - External Interrupt Request 1  (pin D3)          (INT1_vect)
  // - Pin Change Interrupt Request 0 (pins D8 to D13) (PCINT0_vect)
  // - Pin Change Interrupt Request 1 (pins A0 to A5)  (PCINT1_vect)
  // - Pin Change Interrupt Request 2 (pins D0 to D7)  (PCINT2_vect)
  // The external interrupts we want to look at are INT0 and INT1.

  // Set up I/O pin.
  MCUCR &= ~(1 << PUD); // Clear the PUD bit to enable internal pull-ups.
  DDRD &= ~(1 << DDD2); // Clear the Data Direction D2 bit to set the pin to "input".
  PORTD |= (1 << PORTD2); // Raise the PD2 bit to apply internal pull-up to PD2. Does
  // not work if PUD (in MCUCR) is 1

  // Set up External Interrupt registers.

  // Reset External Interrupt Control Register A
  EICRA = 0; // Reset the register.
  EICRA |= (1 << ISC01); // Trigger interrupt on INT0 on a falling edge.
  // NOTE: INT1 has been set to low-level interrupt request.

  // Raise the interrupt flag. This should prevent the interrupt from being triggered
  // on setup.
  EIFR |= (1 << INTF0);

  // Enable the external interrupt.
  EIMSK = (1 << INT0); // Disable INT1, and enable INT0.

  // Clear the interrupt flag.
  //  EIFR &= ~(1 << INTF0);
}

// Interrupt Service Routine for external interrupt 0 (INT0).
ISR(INT0_vect) {
  // Disable external interrupt and enable Timer2 to accommodate the debounce.
  // Disabling INT0 will prevent future triggers during the debounce period,
  // and use Timer2 to determine for how long to remain disabled.
  EIMSK &= ~(1 << INT0); // Disable the external interrupt INT0.
  TIMSK2 |= (1 << OCIE2A); // Enable the debounce timer Timer2.

  const uint8_t ledPin = 13;
  pinMode(ledPin, OUTPUT);
  digitalWrite(ledPin, !digitalRead(ledPin));
}
#endif
