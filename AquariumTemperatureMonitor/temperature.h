#ifndef TEMPERATURE_H
#define TEMPERATURE_H

struct Temperature {
  float temperature1;
  float temperature2;
};


float getAverage(const Temperature * temperature) {
  return (temperature->temperature1 + temperature->temperature2) / 2;
}

void setTemperatures(volatile Temperature * temperature, const float* t1, const float* t2) {
  temperature->temperature1 = *t1;
  temperature->temperature2 = *t2;
}

void clearTemperature(Temperature * temperature) {
  temperature->temperature1 = 0;
  temperature->temperature2 = 0;
}

#endif
