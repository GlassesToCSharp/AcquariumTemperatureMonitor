#ifndef TEMPERATURE_H
#define TEMPERATURE_H

struct Temperature {
  float temperature1;
  float temperature2;
  uint32_t time;
};

// Re-including this file in different files causes a "multiple definitions for
// ..." error. This violates the One Definition Rule. Adding inline to each
// function tells the linker to treat multiple definitions as if they were a
// single definition and pick one definition, thus avoiding the error. More
// info: https://stackoverflow.com/a/12981776

inline float getAverage(const Temperature * temperature) {
  return (temperature->temperature1 + temperature->temperature2) / 2;
}

inline void setTemperatures(volatile Temperature * temperature, const float* t1, const float* t2) {
  temperature->temperature1 = *t1;
  temperature->temperature2 = *t2;
}

inline void setTemperatures(volatile Temperature * dest, const volatile Temperature * src) {
  dest->temperature1 = src->temperature1;
  dest->temperature2 = src->temperature2;
}

inline void setTimestamp(volatile Temperature * dest, const volatile uint32_t timestamp) {
  dest->time = timestamp;
}

inline void clearTemperature(Temperature * temperature) {
  temperature->temperature1 = 0;
  temperature->temperature2 = 0;
}

#endif
