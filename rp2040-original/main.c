#include <stdio.h>
#include "header/project-conf.h"
#include <time.h>
#include <string.h>
#include <float.h>
#include <stdlib.h>
#include <math.h>
#include "pico/stdlib.h"
#include "FreeRTOS.h"
#include "task.h"
#include "timers.h"
#include "semphr.h"
#include "pico/util/datetime.h"
#include "pico/binary_info.h"
#include "pico/bootrom.h"
#include "pico/time.h"
#include "pico/multicore.h"
#include "hardware/gpio.h"
#include "hardware/uart.h"
#include "hardware/rtc.h"
#include "hardware/i2c.h"
#include "hardware/adc.h"
#include "hardware/spi.h"
#include "hardware/dma.h"
#include "hardware/irq.h"
#include "hardware/flash.h"
#include "hardware/sync.h"
#include "hardware/watchdog.h"
#include "hardware/structs/watchdog.h"
#include "hardware/timer.h"
#include "header/defines.h"
#include "header/variables.h"
#include "header/print.h"
#include "header/fifo.h"
#include "header/md5.h"
#include "header/bcc.h"
#include "header/rtc.h"
#include "header/spiflash.h"
#include "header/uart.h"
#include "header/adc.h"

// UART TASK: This task gets uart characters and handles them
void vUARTTask(void *pvParameters)
{
    // Confugiration parameters
    (void)pvParameters;
    uint32_t ulNotificationValue;
    xTaskToNotify_UART = NULL;
    // this buffer stores start time for load profile data
    uint8_t reading_state_start_time[14] = {0};
    // this buffer stores end time for load profile data
    uint8_t reading_state_end_time[14] = {0};

    // This timer deletes rx_buffer if there is no character coming in 5 seconds.
    TimerHandle_t ResetBufferTimer = xTimerCreate(
        "BufferTimer",
        pdMS_TO_TICKS(5000),
        pdFALSE,
        NULL,
        resetRxBuffer);

    // This timer sets state to Greeting(Initial) if there is no request or message in 30 seconds.
    TimerHandle_t ResetStateTimer = xTimerCreate(
        "StateTimer",
        pdMS_TO_TICKS(30000),
        pdFALSE,
        NULL,
        resetState);

    // This timer handles to reboot device if there is no character coming in WriteProgram state. When this timer executes, it means that program data coming from UART is over or cut off
    TimerHandle_t ReprogramTimer = xTimerCreate(
        "ReprogramTimer",
        pdMS_TO_TICKS(5000),
        pdFALSE,
        NULL,
        RebootHandler);

    TimerHandle_t ErrorTimer = xTimerCreate(
        "ErrorTimer",
        pdMS_TO_TICKS(1000),
        pdFALSE,
        NULL,
        sendInvalidMsg);

    while (true)
    {
        xTimerStart(ResetBufferTimer, 0);
        UARTReceive();
        ulNotificationValue = ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
        // If a character comes, this block is going to be executed
        if (ulNotificationValue == 1)
        {
            // Check if UART port is readable
            while (uart_is_readable(UART0_ID))
            {
                // Get character from UART
                uint8_t rx_char = uart_getc(UART0_ID);
                xTimerStart(ErrorTimer, 0);

                // If state is ReProgram then the characters coming from UART are going to handle in this block, because that characters are represent program bytes.
                if (state == ReProgram)
                {
                    xTimerReset(ReprogramTimer, 0);
                    writeProgramToFlash(rx_char);

                    continue;
                }

                PRINTF("%02X\r\n", rx_char);

                rx_buffer[rx_buffer_len++] = rx_char;

                // The end of the message could be '\n' character or a BCC, so this if block checks if the character is '\n' or the whole message is request message according to its length and order of characters
                if (rx_char == '\n' || controlRXBuffer(rx_buffer, rx_buffer_len))
                {
                    // Get the last character of the message and wait for 250 ms. This waiting function is a requirement for the IEC 620256-21 protocol.
                    vTaskDelay(pdMS_TO_TICKS(250));

                    xTimerReset(ResetStateTimer, 0);
                    xTimerStop(ResetBufferTimer, 0);
                    xTimerStop(ErrorTimer, 0);

                    PRINTF("UART TASK: message end and entered the processing area\r\n");
                    PRINTF("UART TASK: rx buffer content: ");
                    printBufferHex(rx_buffer, rx_buffer_len);
                    PRINTF("\r\n");
                    PRINTF("UART TASK: rx buffer len value: %d\r\n", rx_buffer_len);

                    // check if the message is for end connection
                    if (is_end_connection_message(rx_buffer))
                    {
                        resetRxBuffer();
                        resetState();

                        break;
                    }

                    switch (state)
                    {
                    // This is the Initial state in device. In this state, modem and device will handshake.
                    case Greeting:
                        PRINTF("UART TASK: entered greeting state\n");
                        xTimerStart(ResetStateTimer, 0);

                        // Start character of the request message (st_chr_msg) is a protection for message integrity. If there are characters before the greeting message, these characters will be ignored and message will be clear to send to the greeting handler.
                        char *st_chr_msg = strchr((char *)rx_buffer, 0x2F);
                        greetingStateHandler((uint8_t *)st_chr_msg);
                        break;

                    // This state sets baud rate or sends readout message.
                    case Setting:
                        PRINTF("UART TASK: entered setting state\n");
                        xTimerStart(ResetStateTimer, 0);

                        settingStateHandler(rx_buffer, rx_buffer_len);
                        break;

                    // This state handles the request messages for load profile, set date and time, send production info and also before entering WriteProgram state.
                    case Listening:
                        PRINTF("UART TASK: entered listening state\n");
                        xTimerStart(ResetStateTimer, 0);

                        // This switch block checks the request message for which state is going to handled according to message.
                        switch (checkListeningData(rx_buffer, rx_buffer_len))
                        {
                        case DataError:
                            PRINTF("UART TASK: entered listening-dataerror\n");
                            sendErrorMessage((char *)"DATAERROR");
                            break;

                        case BCCError:
                            PRINTF("UART TASK: entered listening-bccerror\n");
                            sendErrorMessage((char *)"BCCERROR");
                            break;

                        // This state represents Load Profile request and send a load profile message for specified dates. If there is no date information, device send all the load profile contents.
                        case Reading:
                            PRINTF("UART TASK: entered listening-reading\n");
                            parseLoadProfileDates(rx_buffer, rx_buffer_len, reading_state_start_time, reading_state_end_time);
                            searchDataInFlash(reading_state_start_time, reading_state_end_time, Reading, ResetStateTimer);
                            break;

                        // This state handles the tasks, timers and sets the state to WriteProgram to start program data handling.
                        case WriteProgram:
                            if (password_correct_flag)
                            {
                                PRINTF("UART TASK: entered listening-reprogram\n");
                                ReProgramHandler();

                                xTimerStop(ResetBufferTimer, 0);
                                xTimerStop(ResetStateTimer, 0);
                                xTimerStart(ReprogramTimer, pdMS_TO_TICKS(100));
                            }
                            else
                            {
                                sendErrorMessage((char *)"PWNOTENTERED");
                            }
                            break;

                        // This state accepts the password and checks. If the password is not correct, time and date in this device cannot be changed.
                        case Password:
                            PRINTF("UART TASK: entered listening-password\n");
                            passwordHandler(rx_buffer);
                            break;

                        // This state changes time of this device.
                        case TimeSet:
                            PRINTF("UART TASK: entered listening-timeset\n");
                            setTimeFromUART(rx_buffer);
                            break;

                        // This state changes date of this device.
                        case DateSet:
                            PRINTF("UART TASK: entered listening-dateset\n");
                            setDateFromUART(rx_buffer);
                            break;

                        // This state sends production info for this device.
                        case ProductionInfo:
                            PRINTF("UART TASK: entered listening-productioninfo\n");
                            sendProductionInfo();
                            break;

                        case SetThreshold:
                            PRINTF("UART TASK: entered listening-setthreshold\n");
                            setThresholdValue(rx_buffer);
                            break;

                        case GetThreshold:
                            PRINTF("UART TASK: entered listening-getthreshold\n");
                            parseThresholdRequestDates(rx_buffer, reading_state_start_time, reading_state_end_time);
                            getThresholdRecord(reading_state_start_time, reading_state_end_time, GetThreshold, ResetStateTimer);
                            break;

                        case ThresholdPin:
                            PRINTF("UART TASK: entered listening-thresholdpin\n");
                            resetThresholdPIN();
                            break;

                        case GetSuddenAmplitudeChange:
                            PRINTF("UART TASK: entered listening-suddenamplitudechange\n");
                            parseACRequestDate(rx_buffer, reading_state_start_time, reading_state_end_time);
                            getSuddenAmplitudeChangeRecords(reading_state_start_time, reading_state_end_time, GetSuddenAmplitudeChange, ResetStateTimer);
                            break;

                        case ReadTime:
                            PRINTF("UART TASK: entered listening-readtime\n");
                            readTime();
                            break;

                        case ReadDate:
                            PRINTF("UART TASK: entered listening-readdate\n");
                            readDate();
                            break;

                        case ReadSerialNumber:
                            PRINTF("UART TASK: entered listening-readserialnumber\n");
                            readSerialNumber();
                            break;

                        case ReadLastVRMSMax:
                            PRINTF("UART TASK: entered listening-readlastvrmsmax\n");
                            sendLastVRMSXValue(ReadLastVRMSMax);
                            break;

                        case ReadLastVRMSMin:
                            sendLastVRMSXValue(ReadLastVRMSMin);
                            break;

                        case ReadLastVRMSMean:
                            sendLastVRMSXValue(ReadLastVRMSMean);
                            break;

                        case ReadResetDates:
                            sendResetDates();
                            break;

                        case GetThresholdObis:
                            sendThresholdObis();
                            break;

                        default:
                            PRINTF("UART TASK: entered listening-default\n");
                            sendErrorMessage((char *)"UNSUPPORTEDLSTMSG");
                            break;
                        }
                        break;
                    case ReProgram:
                        PRINTF("UART TASK: entered reprogram state (UNSUPPORTED!)\n");
                        break;
                    }
                    // After a request or message, buffers and index variables will be set to zero.
                    memset(rx_buffer, 0, 256);
                    rx_buffer_len = 0;
                    PRINTF("UART TASK: buffer content deleted\n");
                }
            }
        }
    }
}

// ADC CONVERTER TASK: This task read ADC PIN to calculate VRMS value and writes a record to flash memory according to current time.
void vADCReadTask()
{
    // Set the parameters for this task.
    struct AmplitudeChangeTimerCallbackParameters ac_data = {0};
    uint8_t amplitude_change_detect_flag = 0;
    // this is a buffer that keeps samples in ADC FIFO in ADC Input 1 to calculate VRMS value
    uint16_t adc_samples_buffer[VRMS_SAMPLE_SIZE];
    float vrms_values_per_second[VRMS_SAMPLE_SIZE / SAMPLE_SIZE_PER_VRMS_CALC];
    uint16_t vrms_buffer_count = 0;
    // vrms buffer values
    float vrms_buffer[VRMS_BUFFER_SIZE] = {0};

    while (1)
    {
        // delay until next cycle
        ulTaskNotifyTake(pdTRUE, portMAX_DELAY);

        if (xSemaphoreTake(xFIFOMutex, portMAX_DELAY) == pdTRUE)
        {
            getLastNElementsToBuffer(&adc_fifo, adc_samples_buffer, VRMS_SAMPLE_SIZE);
            xSemaphoreGive(xFIFOMutex);
        }

        float vrms = calculateVRMS(adc_samples_buffer, VRMS_SAMPLE_SIZE, bias_voltage);
        PRINTF("vrms is: %lf\r\n", vrms);

        uint16_t variance = calculateVariance(adc_samples_buffer, VRMS_SAMPLE_SIZE);
        // PRINTF("variance is: %d\n", variance);

        calculateVRMSValuesPerSecond(vrms_values_per_second, adc_samples_buffer, VRMS_SAMPLE_SIZE, SAMPLE_SIZE_PER_VRMS_CALC, bias_voltage);

        vrms_buffer[(vrms_buffer_count++) % VRMS_BUFFER_SIZE] = vrms;

        if (vrms >= (float)getVRMSThresholdValue())
        {
            setThresholdPIN();
            writeThresholdRecord(vrms, variance);
        }

        if (detectSuddenAmplitudeChangeWithDerivative(vrms_values_per_second, VRMS_SAMPLE_SIZE / SAMPLE_SIZE_PER_VRMS_CALC) || amplitude_change_detect_flag)
        {
            PRINTF("ADC READ TASK: sudden amplitude change detected with Derivate method.\r\n");
            if (amplitude_change_detect_flag)
            {
                writeSuddenAmplitudeChangeRecordToFlash(&ac_data);
                amplitude_change_detect_flag = 0;
            }
            else
            {
                setAmplitudeChangeParameters(&ac_data, vrms_values_per_second, variance, ADC_FIFO_SIZE, sizeof(vrms_values_per_second));
                amplitude_change_detect_flag = 1;
            }
        }

        if (current_time.sec == 0)
        {
            if (current_time.min % load_profile_record_period == 0)
            {
                PRINTF("ADC READ TASK: minute is multiple of %d. write flash block is running...\r\n", load_profile_record_period);

                if (vrms_buffer_count > VRMS_BUFFER_SIZE)
                {
                    vrms_buffer_count = VRMS_BUFFER_SIZE;
                }

                VRMS_VALUES_RECORD vrms_values = vrmsSetMinMaxMean(vrms_buffer, vrms_buffer_count);
                PRINTF("ADC READ TASK: calculated VRMS values.\r\n");

                SPIWriteToFlash(&vrms_values);
                PRINTF("ADC READ TASK: writing flash memory process is completed.\r\n");

                memset(vrms_buffer, 0, VRMS_BUFFER_SIZE * sizeof(float));
                vrms_buffer_count = 0;
                PRINTF("ADC READ TASK: buffer content is deleted\r\n");
            }
        }

        watchdog_update();
    }
}

// DEBUG TASK
void vWriteDebugTask()
{
    TickType_t startTime;
    const TickType_t xFrequency = pdMS_TO_TICKS(1000);
    startTime = xTaskGetTickCount();

    while (true)
    {
        vTaskDelayUntil(&startTime, xFrequency);

        rtc_get_datetime(&current_time);
        datetime_to_str(datetime_str, sizeof(datetime_buffer), &current_time);
        PRINTF("---------------------------------------------------------------------------------------------------------\n");
        PRINTF("WRITE DEBUG TASK: The Time is:%s\r\n", datetime_str);
    }
}

void vPowerLedBlinkTask()
{
    while (1)
    {
        vTaskDelay(pdMS_TO_TICKS(1000));
        gpio_put(POWER_LED_PIN, 0);
        vTaskDelay(pdMS_TO_TICKS(1000));
        gpio_put(POWER_LED_PIN, 1);
    }
}

void vADCSampleTask()
{
    TickType_t startTime;
    const TickType_t xFrequency = 1;
    uint16_t adc_sample;
    uint16_t bias_sample;

    uint16_t bias_buffer[BIAS_SAMPLE_SIZE] = {0};
    uint16_t bias_buffer_count = 0;

    startTime = xTaskGetTickCount();
    while (1)
    {
        adc_sample = adc_read();
        // if (bias_buffer_count % 20 == 0)
        // {
        //     PRINTF("\r\n");
        // }
        // PRINTF("%d,", adc_sample);

        bool is_added = addToFIFO(&adc_fifo, adc_sample);

        if (!is_added)
        {
            removeFirstElementAddNewElement(&adc_fifo, adc_sample);
        }

        bias_sample = adc_read();
        bias_buffer[(bias_buffer_count++) % BIAS_SAMPLE_SIZE] = bias_sample;

        if (bias_buffer_count == BIAS_SAMPLE_SIZE)
        {
            bias_voltage = getMean(bias_buffer, BIAS_SAMPLE_SIZE);
            PRINTF("bias voltage is: %lf\r\n", bias_voltage);

            bias_buffer_count = 0;
            displayFIFOStats(&adc_fifo);

            xTaskNotifyGive(xADCHandle);
        }

        vTaskDelayUntil(&startTime, xFrequency);
    }
}

// RESET TASK: This task sends a pulse to reset PIN.
void vResetTask()
{
    while (1)
    {
        getTimePt7c4338(&current_time);
        rtc_set_datetime(&current_time);
        gpio_put(RESET_PULSE_PIN, 1);
        vTaskDelay(pdMS_TO_TICKS(10));
        gpio_put(RESET_PULSE_PIN, 0);
        vTaskDelay(INTERVAL_MS);
    }
}

int main()
{
    // POWER LED INIT
    gpio_init(POWER_LED_PIN);
    gpio_set_dir(18, GPIO_OUT);
    gpio_put(POWER_LED_PIN, 1);

#if DEBUG
    stdio_init_all();
    sleep_ms(2000);
#endif
    // UART INIT
    if (!initUART())
    {
        return 0;
    }
    gpio_set_function(UART0_TX_PIN, GPIO_FUNC_UART);
    gpio_set_function(UART0_RX_PIN, GPIO_FUNC_UART);

    // RESET INIT
    gpio_init(RESET_PULSE_PIN);
    gpio_set_dir(RESET_PULSE_PIN, GPIO_OUT);

    // THRESHOLD GPIO INIT
    gpio_init(THRESHOLD_PIN);
    gpio_set_dir(THRESHOLD_PIN, GPIO_OUT);
    gpio_put(THRESHOLD_PIN, 0);

    // ADC INIT
    adc_init();
    adc_gpio_init(ADC_READ_PIN);
    adc_gpio_init(ADC_BIAS_PIN);
    // adc_set_clkdiv((1e-3 * 48000000) / 96);
    adc_set_round_robin(0x03);

    // I2C Init
    if (!initI2C())
    {
        return 0;
    }
    gpio_set_function(RTC_I2C_SDA_PIN, GPIO_FUNC_I2C);
    gpio_set_function(RTC_I2C_SCL_PIN, GPIO_FUNC_I2C);

#if WITHOUT_BOOTLOADER
    addSerialNumber();
#endif

    // sector content control
    checkSectorContent();
    checkThresholdContent();

    // // Reset Record Settings
    // resetFlashSettings();

    // FLASH CONTENTS
    getFlashContents();

    // RTC Init
    rtc_init();

    // Get PT7C4338's Time information and set RP2040's RTC module
    if (!getTimePt7c4338(&current_time))
    {
        return 0;
    }

    // If current time which is get from Chip has invalid value, adjust the value.
    if (current_time.dotw < 0 || current_time.dotw > 6)
    {
        current_time.dotw = 2;
    }

    // set and get datetimes from RP2040 RTC's
    bool is_time_set = rtc_set_datetime(&current_time);
    sleep_ms(100);
    bool is_time_get = rtc_get_datetime(&current_time);

    // if time is get correctly, set string datetime.
    if (is_time_get)
    {
        datetime_to_str(datetime_str, sizeof(datetime_buffer), &current_time);
    }
    else
    {
        PRINTF("Time is not GET. Please check the time setting.\n");
    }

    initADCFIFO(&adc_fifo);

    // set when program started
    setProgramStartDate(&current_time);

    if (!setMutexes())
    {
        PRINTF("Failed to set mutexes!\n");
        return 0;
    }

    watchdog_enable(WATCHDOG_TIMEOUT_MS, 0);

    // uint8_t *flash_ac_buf = (uint8_t *)(XIP_BASE + FLASH_AMPLITUDE_CHANGE_OFFSET);
    // uint8_t *flash_ac_buf2 = (uint8_t *)(XIP_BASE + FLASH_AMPLITUDE_CHANGE_OFFSET + FLASH_SECTOR_SIZE);
    // printBufferHex(flash_ac_buf, 20);
    // printBufferHex(flash_ac_buf2, 20);

    // if time is set correctly, start the processes.
    if (is_time_set)
    {
        PRINTF("Time is set. Starting tasks...\n");

        xTaskCreate(vADCReadTask, "ADCReadTask", 6 * 1024, NULL, 3, &xADCHandle);
        xTaskCreate(vUARTTask, "UARTTask", UART_TASK_STACK_SIZE, NULL, 3, &xUARTHandle);
        xTaskCreate(vWriteDebugTask, "WriteDebugTask", 256, NULL, 5, &xWriteDebugHandle);
        xTaskCreate(vResetTask, "ResetTask", 256, NULL, 1, &xResetHandle);
        xTaskCreate(vADCSampleTask, "ADCSampleTask", 3 * 1024, NULL, 3, &xADCSampleHandle);
        xTaskCreate(vPowerLedBlinkTask, "PowerLedBlinkTask", 256, NULL, 1, NULL);

        vTaskCoreAffinitySet(xADCHandle, 1 << 0);
        vTaskCoreAffinitySet(xUARTHandle, 1 << 0);
        vTaskCoreAffinitySet(xADCSampleHandle, 1 << 1);

        vTaskStartScheduler();
    }
    else
    {
        PRINTF("Time is not SET. Please check the time setting.\n");
        return 0;
    }

    while (true)
        ;
}
