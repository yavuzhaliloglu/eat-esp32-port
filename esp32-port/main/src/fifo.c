#include "header/fifo.h"
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "header/bcc.h"

// dev branch'teki blink/src/fifo.c dosyasindan uyarlanmistir.
// MANTIK BIREBIR AYNI - sadece Pico SDK/FreeRTOS include yollari ESP-IDF'e
// gore duzeltildi ("pico/stdlib.h" kaldirildi, "FreeRTOS.h"/"semphr.h"
// "freertos/" alt klasorunden aliniyor). xSemaphoreTake/xSemaphoreGive
// cagrilari, mutex handle'lari, 250ms timeout + LED hata deseni davranisi
// hic degismedi.

void initADCFIFO(ADC_FIFO *f)
{
    f->head = 0;
    f->tail = 0;
    f->count = 0;
    memset(f->data, 0, ADC_FIFO_SIZE * sizeof(uint16_t));
}

bool isFIFOFull(ADC_FIFO *f)
{
    return (f->count == ADC_FIFO_SIZE);
}

bool isFIFOEmpty(ADC_FIFO *f)
{
    return (f->count == 0);
}

bool addToFIFO(ADC_FIFO *f, uint16_t data)
{
    bool result = false;

    if (xSemaphoreTake(xFIFOMutex, pdMS_TO_TICKS(250)) == pdTRUE)
    {
        if (!isFIFOFull(f))
        {
            f->data[f->tail] = data;
            f->tail = (f->tail + 1) % ADC_FIFO_SIZE;
            f->count++;
            result = true;
        }

        xSemaphoreGive(xFIFOMutex);
    }
    else
    {
        led_blink_pattern(LED_ERROR_CODE_FIFO_MUTEX_NOT_TAKEN, false);
    }

    return result;
}

bool removeFromFIFO(ADC_FIFO *f)
{
    bool result = false;

    if (xSemaphoreTake(xFIFOMutex, pdMS_TO_TICKS(250)) == pdTRUE)
    {
        if (!isFIFOEmpty(f))
        {
            f->head = (f->head + 1) % ADC_FIFO_SIZE;
            f->count--;
            result = true;
        }

        xSemaphoreGive(xFIFOMutex);
    }
    else
    {
        led_blink_pattern(LED_ERROR_CODE_FIFO_MUTEX_NOT_TAKEN, false);
    }

    return result;
}

bool removeFirstElementAddNewElement(ADC_FIFO *f, uint16_t data)
{
    bool is_removed_from_fifo = removeFromFIFO(f);
    if (is_removed_from_fifo)
    {
        bool is_added_to_fifo = addToFIFO(f, data);
        if (is_added_to_fifo)
        {
            return true;
        }
    }

    return false;
}

// Function to get the last N elements of the FIFO
void getLastNElementsToBuffer(ADC_FIFO *f, uint16_t *buffer, uint16_t count)
{
    if (count > f->count)
    {
        count = f->count;
    }
    for (uint16_t i = 0; i < count; i++)
    {
        buffer[i] = f->data[(f->tail - count + i + ADC_FIFO_SIZE) % ADC_FIFO_SIZE];
    }
}
