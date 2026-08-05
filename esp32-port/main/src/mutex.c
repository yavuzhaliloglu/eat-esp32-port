#include "header/mutex.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "header/project_globals.h"
#include "header/bcc.h"

// dev branch'teki blink/src/mutex.c dosyasindan uyarlanmistir.
// MANTIK BIREBIR AYNI - sadece Pico SDK/FreeRTOS include yollari duzeltildi.
// Tum mutex'ler 250ms sinirli timeout ile bekliyor, alamazsa ilgili LED
// hata deseni tetikleniyor (sonsuza kadar takili kalma riski yok).

uint16_t getVRMSThresholdValue()
{
    uint16_t vrms_th_val = 0;
    if (xSemaphoreTake(xVRMSThresholdMutex, pdMS_TO_TICKS(250)) == pdTRUE)
    {
        vrms_th_val = vrms_threshold;
        xSemaphoreGive(xVRMSThresholdMutex);
    }
    else
    {
        led_blink_pattern(LED_ERROR_CODE_VRMS_THRESHOLD_MUTEX_NOT_TAKEN, false);
    }

    return vrms_th_val;
}

void setVRMSThresholdValue(uint16_t value)
{
    if (xSemaphoreTake(xVRMSThresholdMutex, pdMS_TO_TICKS(250)) == pdTRUE)
    {
        vrms_threshold = value;
        xSemaphoreGive(xVRMSThresholdMutex);
    }
    else
    {
        led_blink_pattern(LED_ERROR_CODE_VRMS_THRESHOLD_MUTEX_NOT_TAKEN, false);
    }
}

#if CONF_THRESHOLD_PIN_ENABLED
uint8_t getThresholdSetBeforeFlag()
{
    uint8_t th_set_flag = 0;
    if (xSemaphoreTake(xThresholdSetFlagMutex, pdMS_TO_TICKS(250)) == pdTRUE)
    {
        th_set_flag = threshold_set_before;
        xSemaphoreGive(xThresholdSetFlagMutex);
    }
    else
    {
        led_blink_pattern(LED_ERROR_CODE_THRESHOLD_SET_MUTEX_NOT_TAKEN, false);
    }

    return th_set_flag;
}

void setThresholdSetBeforeFlag(uint8_t value)
{
    if (xSemaphoreTake(xThresholdSetFlagMutex, pdMS_TO_TICKS(250)) == pdTRUE)
    {
        threshold_set_before = value;
        xSemaphoreGive(xThresholdSetFlagMutex);
    }
    else
    {
        led_blink_pattern(LED_ERROR_CODE_THRESHOLD_SET_MUTEX_NOT_TAKEN, false);
    }
}
#endif

uint8_t setMutexes()
{
    xFlashMutex = xSemaphoreCreateMutex();
    if (xFlashMutex == NULL)
    {
        PRINTF("Flash mutex is not created.\n");
        return 0;
    }
    xFIFOMutex = xSemaphoreCreateMutex();
    if (xFIFOMutex == NULL)
    {
        PRINTF("FIFO mutex is not created.\n");
        return 0;
    }
    xVRMSLastValuesMutex = xSemaphoreCreateMutex();
    if (xVRMSLastValuesMutex == NULL)
    {
        PRINTF("VRMSLastValues mutex is not created.\n");
        return 0;
    }
    xVRMSThresholdMutex = xSemaphoreCreateMutex();
    if (xVRMSThresholdMutex == NULL)
    {
        PRINTF("VRMSThreshold mutex is not created.\n");
        return 0;
    }
    xThresholdSetFlagMutex = xSemaphoreCreateMutex();
    if (xThresholdSetFlagMutex == NULL)
    {
        PRINTF("ThresholdSetFlag mutex is not created.\n");
        return 0;
    }

    return 1;
}
