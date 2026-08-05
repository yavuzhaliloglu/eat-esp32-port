#ifndef MUTEX_H
#define MUTEX_H

#include <stdint.h>
#include "header/project_conf.h" // CONF_THRESHOLD_PIN_ENABLED icin

// dev branch'teki blink/header/mutex.h ile ayni. CONF_THRESHOLD_PIN_ENABLED
// bizde 1 (acik) oldugu icin getThresholdSetBeforeFlag/setThresholdSetBeforeFlag
// gercekten derlenecek (dev'de varsayilan kapaliydi).

uint8_t setMutexes();
uint16_t getVRMSThresholdValue();
void setVRMSThresholdValue(uint16_t value);
#if CONF_THRESHOLD_PIN_ENABLED
uint8_t getThresholdSetBeforeFlag();
void setThresholdSetBeforeFlag(uint8_t value);
#endif

#endif // MUTEX_H
