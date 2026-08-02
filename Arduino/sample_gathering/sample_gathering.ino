#include <Wire.h>
#include "MAX30105.h"

#define serial Serial

MAX30105 sensor;
const int SAMPLE_RATE     = 100;
const int PULSE_WIDTH     = 411;
const int ADC_RANGE       = 4096;
const byte LED_MODE       = 2;
const byte POWER_LEVEL    = 0x3F;
const byte SAMPLE_AVERAGE = 1;

void setup() {
  // put your setup code here, to run once:
  Serial.begin(115200);

  if (sensor.begin() == false)
  {
    serial.println("couldn't initilaize the sensor");
    exit(0);
  }
  sensor.setup(POWER_LEVEL, SAMPLE_AVERAGE, LED_MODE, SAMPLE_RATE, PULSE_WIDTH, ADC_RANGE);
}

void loop() {
  // put your main code here, to run repeatedly:
  delay(5000);

  for (int i = 0; i < 5000; i++)
    serial.println(sensor.getIR());

  delay(10000);
}
