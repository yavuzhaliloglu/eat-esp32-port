#ifndef FIFO_H
#define FIFO_H

#include <stdint.h>
#include <stdbool.h>
#include "header/project_globals.h" // ADC_FIFO struct tanimi icin

// dev branch'teki blink/header/fifo.h ile ayni (sadece include yolu
// "header/project_globals.h" olarak duzeltildi, digerlerinde kullandigimiz
// konvansiyona uysun diye).

void initADCFIFO(ADC_FIFO *f);
bool isFIFOFull(ADC_FIFO *f);
bool isFIFOEmpty(ADC_FIFO *f);
bool addToFIFO(ADC_FIFO *f, uint16_t data);
bool removeFromFIFO(ADC_FIFO *f);
bool removeFirstElementAddNewElement(ADC_FIFO *f, uint16_t data);
void getLastNElementsToBuffer(ADC_FIFO *f, uint16_t *buffer, uint16_t count);
#endif
