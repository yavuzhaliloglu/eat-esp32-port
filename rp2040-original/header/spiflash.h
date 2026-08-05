#ifndef SPIFLASH_H
#define SPIFLASH_H

// this functions gets a buffer and adds last element's bits to rest of buffer's elements
void convertTo8Bit(uint8_t *buffer, uint8_t len)
{
    // get last element of the buffer
    uint8_t lsb_byte = buffer[len - 1];

    for (uint8_t i = 0; i < len - 1; i++)
    {
        // shift buffer's element to left
        buffer[i] = (buffer[i] << 1);
        // get the last element's LSB
        uint8_t lsb = lsb_byte & 0x01;
        // add the LSB to buffer element
        buffer[i] += lsb;
        // shift the last element to right to get next bit to add next buffer element
        lsb_byte = (lsb_byte >> 1);
    }
}

// this function writes 7-bit values rpb buffer and if buffer size is equals to block of rpb data size, (7 * 256 bytes) the buffer is written to flash.
void writeBlock(uint8_t *buffer, uint8_t size)
{
    // Convert 7-bit values to 8-bit
    convertTo8Bit(buffer, size);

    // if data_cnt value is 0 and this function runs, it means program is over and there are program data in rpb buffer which is not written to flash, and there is no data in data_pck buffer.
    // So there is no need to copy data_pck values to rpb buffer.
    if (data_cnt != 0)
    {
        memcpy(rpb + (rpb_len), buffer, size - 1);
    }

    // increase the rpb_len value to write next 7-bytes area
    rpb_len += 7;

    // is_program_end flag is set when there is no values coming in 5 seconds from UART, and it means program data is over or program data transfer broke.
    if (rpb_len == FLASH_RPB_BLOCK_SIZE || is_program_end)
    {
        PRINTF("WRITEBLOCK: rpb len is equal to block size. Programming...\n");

        if (xSemaphoreTake(xFlashMutex, portMAX_DELAY) == pdTRUE)
        {
            // write the buffer to flash
            flash_range_program(FLASH_REPROGRAM_OFFSET + (ota_block_count * FLASH_RPB_BLOCK_SIZE), rpb, FLASH_RPB_BLOCK_SIZE);
            // jump to next block offset to write data automatically

            xSemaphoreGive(xFlashMutex);
        }
        ota_block_count++;
        // reset buffer and length value of rpb.
        memset(rpb, 0, FLASH_RPB_BLOCK_SIZE);
        rpb_len = 0;
    }
}

// this function gets the character coming from UART and adds it to data_pck buffer and copies it to rpb buffer.
void writeProgramToFlash(uint8_t chr)
{
    // copy character to data_pack value. If is_program_end flag is set it means there is no character coming from UART the no need to copy a NULL value to data_pack buffer.
    if (!is_program_end)
    {
        data_pck[data_cnt++] = chr;
    }

    // if data_pck buffer is full or is_program_end flag is set the buffer can be copied to rpb buffer
    if (data_cnt == 8 || is_program_end)
    {
        // copy the data_pck buffer to rpb buffer
        writeBlock(data_pck, data_cnt);

        // reset the data_pck buffer and its length value
        memset(data_pck, 0, 8);
        data_cnt = 0;
    }
}

// This function gets the contents like sector data, last records contents from flash and sets them to variables.
void getFlashContents()
{
    // get sector count of records
    uint16_t *flash_sector_content = (uint16_t *)(XIP_BASE + FLASH_SECTOR_OFFSET);
    sector_data = flash_sector_content[0];

    // get record data
    uint8_t *flash_data_contents = (uint8_t *)(XIP_BASE + FLASH_DATA_OFFSET + (sector_data * FLASH_SECTOR_SIZE));
    memcpy(flash_data, flash_data_contents, FLASH_SECTOR_SIZE);

    // get threshold data
    uint16_t *th_ptr = (uint16_t *)(XIP_BASE + FLASH_THRESHOLD_INFO_OFFSET);
    vrms_threshold = th_ptr[0];
    th_sector_data = th_ptr[1];

    // set serial number
    uint8_t *serial_number_offset = (uint8_t *)(XIP_BASE + FLASH_SERIAL_OFFSET);
    memcpy(serial_number, serial_number_offset, SERIAL_NUMBER_SIZE);

    PRINTF("GETFLASHCONTENTS: vrms threshold value is: %d\n", vrms_threshold);
    PRINTF("GETFLASHCONTENTS: flash sector is: %d\n", sector_data);
    PRINTF("GETFLASHCONTENTS: threshold sector is: %d\n", th_sector_data);
}

// This function writes current sector data to flash.
void __not_in_flash_func(setSectorData)(uint16_t sector_value)
{
    uint16_t sector_buffer[FLASH_PAGE_SIZE / sizeof(uint16_t)] = {0};
    sector_buffer[0] = sector_value;

    PRINTF("SETSECTORDATA: sector data which is going to be written: %d\n", sector_value);

    if (xSemaphoreTake(xFlashMutex, portMAX_DELAY) == pdTRUE)
    {
        PRINTF("SETSECTORDATA: write flash mutex received\n");
        flash_range_erase(FLASH_SECTOR_OFFSET, FLASH_SECTOR_SIZE);
        flash_range_program(FLASH_SECTOR_OFFSET, (uint8_t *)sector_buffer, FLASH_PAGE_SIZE);
        xSemaphoreGive(xFlashMutex);
    }
    else
    {
        PRINTF("MUTEX CANNOT RECEIVED!\n");
    }
}

// This function converts the datetime value to char array as characters to write flash correctly
void setDateToCharArray(int value, char *array)
{
    // if value is smaller than 10, second character of array will be always 0, first element of array will be value
    if (value < 10)
    {
        array[0] = '0';
        array[1] = value + '0';
    }
    // if value is bigger than 10, two character will be written to char array, first element of array keeps ones place, second element of array keeps tens place
    else
    {
        array[0] = value / 10 + '0';
        array[1] = value % 10 + '0';
    }
}

// this function converts a float value's floating value to uint8_t value.
uint8_t floatDecimalDigitToUint8t(float float_value)
{
    // get floating value of float value and multiply it with 10, so we can get the first digit. We set that value an uint8_t value because we don't want to get rest of the floating digits.
    uint8_t floating_value = (float_value - (float)floor(float_value)) * 10;

    PRINTF("float value before subtraction: %lf\n", float_value);
    PRINTF("floating value after floor and uint8_t: %d\n\n", floating_value);

    return floating_value;
}

// This function gets a buffer which includes VRMS values, and calculate the max, min and mean values of this buffer and sets the variables.
VRMS_VALUES_RECORD vrmsSetMinMaxMean(float *buffer, uint16_t size)
{
    float buffer_max = buffer[0];
    float buffer_min = buffer[0];
    float buffer_sum = buffer[0];
    VRMS_VALUES_RECORD vrms_values;

    for (uint16_t i = 1; i < size; i++)
    {
        if (buffer[i] > buffer_max)
        {
            buffer_max = buffer[i];
        }

        if (buffer[i] < buffer_min)
        {
            buffer_min = buffer[i];
        }

        buffer_sum += buffer[i];
    }

    vrms_values.vrms_max = (uint8_t)floor(buffer_max);
    vrms_values.vrms_min = (uint8_t)floor(buffer_min);
    vrms_values.vrms_max_dec = floatDecimalDigitToUint8t(buffer_max);
    vrms_values.vrms_min_dec = floatDecimalDigitToUint8t(buffer_min);

    if (size == 0)
    {
        vrms_values.vrms_mean = 0;
        vrms_values.vrms_mean_dec = 0;
    }
    else
    {
        vrms_values.vrms_mean = (uint8_t)(buffer_sum / size);
        vrms_values.vrms_mean_dec = floatDecimalDigitToUint8t(buffer_sum / size);
    }

    PRINTF("buffer max: %lf, vrms_max: %d, vrms_max_dec: %d\n", buffer_max, vrms_values.vrms_max, vrms_values.vrms_max_dec);
    PRINTF("buffer min: %lf, vrms_min: %d, vrms_min_dec: %d\n", buffer_min, vrms_values.vrms_min, vrms_values.vrms_min_dec);
    PRINTF("buffer mean: %lf, vrms_mean: %d, vrms_mean_dec: %d\n", buffer_sum / size, vrms_values.vrms_mean, vrms_values.vrms_mean_dec);

    return vrms_values;
}

float convertVRMSValueToFloat(uint8_t value, uint8_t value_dec)
{
    return value + value_dec / 10.0;
}

// This function sets the current time values which are 16 bytes total and calculated VRMS values to flash
void setFlashData(VRMS_VALUES_RECORD *vrms_values)
{
    // initialize the variables
    struct FlashData data;
    uint8_t *flash_data_contents = (uint8_t *)(XIP_BASE + FLASH_DATA_OFFSET + (sector_data * FLASH_SECTOR_SIZE));
    uint16_t offset = 0;

    // set date values and VRMS values to FlashData struct variable
    setDateToCharArray(current_time.year, data.year);
    setDateToCharArray(current_time.month, data.month);
    setDateToCharArray(current_time.day, data.day);
    setDateToCharArray(current_time.hour, data.hour);
    setDateToCharArray(current_time.min, data.min);
    data.max_volt = vrms_values->vrms_max;
    data.min_volt = vrms_values->vrms_min;
    data.mean_volt = vrms_values->vrms_mean;
    data.max_volt_dec = vrms_values->vrms_max_dec;
    data.min_volt_dec = vrms_values->vrms_min_dec;
    data.mean_volt_dec = vrms_values->vrms_mean_dec;

    if (xSemaphoreTake(xVRMSLastValuesMutex, portMAX_DELAY) == pdTRUE)
    {
        // convert last record vrms values to float
        vrms_max_last = convertVRMSValueToFloat(vrms_values->vrms_max, vrms_values->vrms_max_dec);
        vrms_min_last = convertVRMSValueToFloat(vrms_values->vrms_min, vrms_values->vrms_min_dec);
        vrms_mean_last = convertVRMSValueToFloat(vrms_values->vrms_mean, vrms_values->vrms_mean_dec);

        xSemaphoreGive(xVRMSLastValuesMutex);
    }

    if (xSemaphoreTake(xFlashMutex, portMAX_DELAY) == pdTRUE)
    {
        PRINTF("SETFLASHDATA: offset loop mutex received\n");
        // find the last offset of flash records and write current values to last offset of flash_data buffer
        for (offset = 0; offset < FLASH_SECTOR_SIZE; offset += FLASH_RECORD_SIZE)
        {
            if (flash_data_contents[offset] == '\0' || flash_data_contents[offset] == 0xff)
            {
                if (offset == 0)
                {
                    PRINTF("SETFLASHDATA: last record is not found.\n");
                }
                else
                {
                    PRINTF("SETFLASHDATA: last record is start in %d offset\n", offset - 16);
                }

                flash_data[offset / FLASH_RECORD_SIZE] = data;

                PRINTF("SETFLASHDATA: record saved to offset: %d. used %d/%d of sector.\n", offset, offset + 16, FLASH_SECTOR_SIZE);

                break;
            }
        }

        xSemaphoreGive(xFlashMutex);
    }

    // if offset value is equals or bigger than FLASH_SECTOR_SIZE, (4096 bytes) it means current sector is full and program should write new values to next sector
    if (offset >= FLASH_SECTOR_SIZE)
    {
        PRINTF("SETFLASHDATA: offset value is equals to sector size. Current sector data is: %d. Sector is changing...\n", sector_data);

        // if current sector is last sector of flash, sector data will be 0 and the program will start to write new records to beginning of the flash record offset
        if (sector_data == FLASH_TOTAL_SECTORS)
        {
            sector_data = 0;
        }
        else
        {
            sector_data++;
        }

        PRINTF("SETFLASHDATA: new sector value is: %d\n", sector_data);

        // reset variables and call setSectorData()
        memset(flash_data, 0, FLASH_SECTOR_SIZE);
        flash_data[0] = data;
        setSectorData(sector_data);

        PRINTF("SETFLASHDATA: Sector changing written to flash.\n");
    }
}

// This function writes flash_data content to flash area
void __not_in_flash_func(SPIWriteToFlash)(VRMS_VALUES_RECORD *vrms_values)
{
    PRINTF("SPIWRITETOFLASH: Setting flash data...\n");

    setFlashData(vrms_values);
    // setSectorData();

    if (xSemaphoreTake(xFlashMutex, portMAX_DELAY) == pdTRUE)
    {
        PRINTF("SPIWRITETOFLASH: write flash mutex received\n");
        flash_range_erase(FLASH_DATA_OFFSET + (sector_data * FLASH_SECTOR_SIZE), FLASH_SECTOR_SIZE);
        flash_range_program(FLASH_DATA_OFFSET + (sector_data * FLASH_SECTOR_SIZE), (uint8_t *)flash_data, FLASH_SECTOR_SIZE);
        xSemaphoreGive(xFlashMutex);
    }
    else
    {
        PRINTF("MUTEX CANNOT RECEIVED!\n");
    }
}

// this function converts an array to datetime value
void arrayToDatetime(datetime_t *dt, uint8_t *arr)
{
    dt->year = (arr[0] - '0') * 10 + (arr[1] - '0');
    dt->month = (arr[2] - '0') * 10 + (arr[3] - '0');
    dt->day = (arr[4] - '0') * 10 + (arr[5] - '0');
    dt->hour = (arr[6] - '0') * 10 + (arr[7] - '0');
    dt->min = (arr[8] - '0') * 10 + (arr[9] - '0');
}

// this function converts an array to datetime value
void arrayToDatetimeWithSecond(datetime_t *dt, uint8_t *arr)
{
    dt->year = (arr[0] - '0') * 10 + (arr[1] - '0');
    dt->month = (arr[2] - '0') * 10 + (arr[3] - '0');
    dt->day = (arr[4] - '0') * 10 + (arr[5] - '0');
    dt->hour = (arr[6] - '0') * 10 + (arr[7] - '0');
    dt->min = (arr[8] - '0') * 10 + (arr[9] - '0');
    dt->sec = (arr[10] - '0') * 10 + (arr[11] - '0');
}

// This functon compares two datetime values and return an int value
int datetimeComp(datetime_t *dt1, datetime_t *dt2)
{
    if (dt1->year - dt2->year != 0)
    {
        return dt1->year - dt2->year;
    }

    else if (dt1->month - dt2->month != 0)
    {
        return dt1->month - dt2->month;
    }

    else if (dt1->day - dt2->day != 0)
    {
        return dt1->day - dt2->day;
    }

    else if (dt1->hour - dt2->hour != 0)
    {
        return dt1->hour - dt2->hour;
    }

    else if (dt1->min - dt2->min != 0)
    {
        return dt1->min - dt2->min;
    }

    else if (dt1->sec - dt2->sec != 0)
    {
        return dt1->sec - dt2->sec;
    }

    return 0;
}

// This function copies a datetime value to another datetime value
void datetimeCopy(datetime_t *src, datetime_t *dst)
{
    dst->year = src->year;
    dst->month = src->month;
    dst->day = src->day;
    dst->dotw = src->dotw;
    dst->hour = src->hour;
    dst->min = src->min;
    dst->sec = src->sec;
}

void getAllRecords(int32_t *st_idx, int32_t *end_idx, datetime_t *start, datetime_t *end, size_t offset, size_t size, uint16_t record_size)
{
    uint8_t *flash_start_content = (uint8_t *)(XIP_BASE + offset);

    if (xSemaphoreTake(xFlashMutex, portMAX_DELAY) == pdTRUE)
    {
        PRINTF("GETALLRECORDS: offset loop mutex received\n");
        for (unsigned int i = 0; i < size; i += record_size)
        {
            // this is the end index control. if start index occurs and current index starts with FF or current index is NULL which means this is the last index of records
            if ((*st_idx != -1) && (flash_start_content[i] == 0xFF || flash_start_content[i] == 0x00))
            {
                arrayToDatetime(end, &flash_start_content[i - record_size]);
                *end_idx = i - record_size;
                break;
            }

            // if current index is 0xFF or 0x00, this is an empty record and no need to look for if it is start index, continue the loop
            if (flash_start_content[i] == 0xFF || flash_start_content[i] == 0x00)
            {
                continue;
            }

            // if current record is not empty, this is start index of records.
            if (*st_idx == -1)
            {
                arrayToDatetime(start, &flash_start_content[i]);
                *st_idx = i;
            }
        }

        xSemaphoreGive(xFlashMutex);
    }
}

void getSelectedRecords(int32_t *st_idx, int32_t *end_idx, datetime_t *start, datetime_t *end, datetime_t *dt_start, datetime_t *dt_end, uint8_t *reading_state_start_time, uint8_t *reading_state_end_time, size_t offset, size_t size, uint16_t record_size, enum ListeningStates state)
{
    uint8_t *flash_start_content = (uint8_t *)(XIP_BASE + offset);

    // convert date and time values to datetime value
    if (state == GetThreshold || state == GetSuddenAmplitudeChange)
    {
        arrayToDatetimeWithSecond(start, reading_state_start_time);
        arrayToDatetimeWithSecond(end, reading_state_end_time);
    }
    else
    {
        arrayToDatetime(start, reading_state_start_time);
        arrayToDatetime(end, reading_state_end_time);
    }

    // if start date is bigger than end date, it means dates are wrong so function returns
    if (datetimeComp(start, end) > 0)
    {
        return;
    }

    if (xSemaphoreTake(xFlashMutex, portMAX_DELAY) == pdTRUE)
    {
        for (uint32_t i = 0; i < size; i += record_size)
        {
            // initialize the current datetime
            datetime_t recurrent_time = {0};

            // PRINTF("GETSELECTEDRECORDS: offset loop mutex received\n");
            // if current index is empty, continue
            if (flash_start_content[i] == 0xFF || flash_start_content[i] == 0x00)
            {
                continue;
            }

            // if current index is not empty, set datetime to current index record
            if (state == GetThreshold || state == GetSuddenAmplitudeChange)
            {
                arrayToDatetimeWithSecond(&recurrent_time, &flash_start_content[i]);
            }
            else
            {
                arrayToDatetime(&recurrent_time, &flash_start_content[i]);
            }

            // if current record datetime  is bigger than start datetime and start index is not set, this is the start index
            if (datetimeComp(&recurrent_time, start) >= 0)
            {
                if (*st_idx == -1 || (datetimeComp(&recurrent_time, dt_start) < 0))
                {
                    *st_idx = i;
                    datetimeCopy(&recurrent_time, dt_start);
                }
            }

            // if current record datetime is smaller than end datetime and end index is not set, this is the end index
            if (datetimeComp(&recurrent_time, end) <= 0)
            {
                if (*end_idx == -1 || datetimeComp(&recurrent_time, dt_end) > 0)
                {
                    *end_idx = i;
                    datetimeCopy(&recurrent_time, dt_end);
                }
            }
        }

        xSemaphoreGive(xFlashMutex);
    }
}

// This function searches the requested data in flash by starting from flash record beginning offset, collects data from flash and sends it to UART to show load profile content
void searchDataInFlash(uint8_t *reading_state_start_time, uint8_t *reading_state_end_time, enum ListeningStates state, TimerHandle_t timer)
{
    // initialize the variables
    datetime_t start = {0};
    datetime_t end = {0};
    datetime_t dt_start = {0};
    datetime_t dt_end = {0};
    int32_t start_index = -1;
    int32_t end_index = -1;
    char load_profile_line[41] = {0};
    uint8_t *flash_start_content = (uint8_t *)(XIP_BASE + FLASH_DATA_OFFSET);
    uint8_t *date_start = (uint8_t *)strchr((char *)rx_buffer, '(');
    uint8_t *date_end = (uint8_t *)strchr((char *)rx_buffer, ')');
    char year[3] = {0};
    char month[3] = {0};
    char day[3] = {0};
    char hour[3] = {0};
    char minute[3] = {0};
    uint8_t max = 0;
    uint8_t max_dec = 0;
    uint8_t min = 0;
    uint8_t min_dec = 0;
    uint8_t mean = 0;
    uint8_t mean_dec = 0;

    // if there are just ; character between parentheses and this function executes, it means load profile request got without dates so it means all records will be showed in load profile request
    if (date_end - date_start == 2)
    {
        PRINTF("SEARCHDATAINFLASH: all records are going to send\n");
        getAllRecords(&start_index, &end_index, &start, &end, FLASH_DATA_OFFSET, FLASH_TOTAL_RECORDS, FLASH_RECORD_SIZE);
    }
    // if rx_buffer_len is not 14, request got with dates and records will be showed between those dates.
    else
    {
        PRINTF("SEARCHDATAINFLASH: selected records are going to send\n");
        getSelectedRecords(&start_index, &end_index, &start, &end, &dt_start, &dt_end, reading_state_start_time, reading_state_end_time, FLASH_DATA_OFFSET, FLASH_TOTAL_RECORDS, FLASH_RECORD_SIZE, state);
    }

    PRINTF("SEARCHDATAINFLASH: Start index is: %ld\n", start_index);
    PRINTF("SEARCHDATAINFLASH: End index is: %ld\n", end_index);

    // if start index is bigger than end index, swap the values
    if (start_index > end_index)
    {
        uint32_t temp = start_index;
        start_index = end_index;
        end_index = temp;
    }

    // if there start and end index are set, there are records between these times so send them to UART
    if (start_index >= 0 && end_index >= 0)
    {
        PRINTF("SEARCHDATAINFLASH: Generating messages...\n");

        // initialize the variables
        uint8_t xor_result = 0x00;
        uint32_t start_addr = start_index;
        uint32_t end_addr = start_index <= end_index ? end_index : 1572864;
        int result;

        // send STX character
        uart_putc(UART0_ID, STX);

        for (; start_addr <= end_addr;)
        {
            if (xSemaphoreTake(xFlashMutex, portMAX_DELAY) == pdTRUE)
            {
                PRINTF("SEARCHDATAINFLASH: set data mutex received\n");

                // set char arrays to initialize a string for record
                snprintf(year, sizeof(year), "%c%c", flash_start_content[start_addr], flash_start_content[start_addr + 1]);
                snprintf(month, sizeof(month), "%c%c", flash_start_content[start_addr + 2], flash_start_content[start_addr + 3]);
                snprintf(day, sizeof(day), "%c%c", flash_start_content[start_addr + 4], flash_start_content[start_addr + 5]);
                snprintf(hour, sizeof(hour), "%c%c", flash_start_content[start_addr + 6], flash_start_content[start_addr + 7]);
                snprintf(minute, sizeof(minute), "%c%c", flash_start_content[start_addr + 8], flash_start_content[start_addr + 9]);
                max = flash_start_content[start_addr + 10];
                max_dec = flash_start_content[start_addr + 11];
                min = flash_start_content[start_addr + 12];
                min_dec = flash_start_content[start_addr + 13];
                mean = flash_start_content[start_addr + 14];
                mean_dec = flash_start_content[start_addr + 15];

                xSemaphoreGive(xFlashMutex);
            }

            result = snprintf(load_profile_line, sizeof(load_profile_line), "(%s-%s-%s,%s:%s)(%03d.%d,%03d.%d,%03d.%d)\r\n", year, month, day, hour, minute, min, min_dec, max, max_dec, mean, mean_dec);
            bccGenerate((uint8_t *)load_profile_line, 40, &xor_result);
            PRINTF("SEARCHDATAINFLASH: message to send: %s\n", load_profile_line);

            // send the record to UART and wait
            PRINTF("SEARCHDATAINFLASH: Record to send as bytearray: \n");
            printBufferHex((uint8_t *)load_profile_line, 41);
            PRINTF("\n");

            if (result >= (int)sizeof(load_profile_line))
            {
                PRINTF("SEARCHDATAINFLASH: Buffer Overflow! Sending NACK.\n");
                sendErrorMessage((char *)"LPBUFFEROVERFLOW");
            }
            else
            {
                uart_puts(UART0_ID, load_profile_line);
            }

            // if start address equals to end address, it means there is just one record to send or this record is the last record to send
            if (start_addr == end_addr)
            {
                result = snprintf(load_profile_line, sizeof(load_profile_line), "\r%c", ETX);
                bccGenerate((uint8_t *)load_profile_line, result, &xor_result);

                uart_puts(UART0_ID, load_profile_line);
                PRINTF("SEARCHDATAINFLASH: lp data block xor is: %02X\n", xor_result);
                uart_putc(UART0_ID, xor_result);
            }

            vTaskDelay(pdMS_TO_TICKS(15));
            xTimerReset(timer, 0);

            // last sector and record control
            if (start_index > end_index && start_addr == 1572848)
            {
                start_addr = 0;
                end_addr = end_index;
            }
            // jump to next record
            else
                start_addr += FLASH_RECORD_SIZE;
        }
    }
    // if start record or end record does not exist, send NACK message
    else
    {
        PRINTF("SEARCHDATAINFLASH: data not found.\n");
        sendErrorMessage((char *)"NODATAFOUND");
    }

    // reset time buffers
    memset(reading_state_start_time, 0, 10);
    memset(reading_state_end_time, 0, 10);

    PRINTF("SEARCHDATAINFLASH: time values are deleted.\n");
}

// This function resets records and sector data and set sector data to 0 (UNUSUED FUNCTION)
void __not_in_flash_func(resetFlashSettings)()
{
    PRINTF("RESETFLASHSETTINGS: resetting records anad sector values\n");

    uint16_t reset_flash[FLASH_PAGE_SIZE / sizeof(uint16_t)] = {0};

    uint32_t ints = save_and_disable_interrupts();
    flash_range_erase(FLASH_SECTOR_OFFSET, FLASH_SECTOR_SIZE);
    flash_range_program(FLASH_SECTOR_OFFSET, (uint8_t *)reset_flash, FLASH_PAGE_SIZE);

    flash_range_erase(FLASH_DATA_OFFSET, FLASH_TOTAL_SECTORS * FLASH_SECTOR_SIZE);
    restore_interrupts(ints);

    PRINTF("RESETFLASHSETTINGS: erasing is successful.\n");
}

void __not_in_flash_func(checkSectorContent)()
{
    uint16_t *flash_sector_content = (uint16_t *)(XIP_BASE + FLASH_SECTOR_OFFSET);

    if (flash_sector_content[0] == 0xFFFF)
    {
        PRINTF("CHECKSECTORCONTENT: sector area is empty. Sector content is going to set 0.\n");
        uint16_t sector_buffer[FLASH_PAGE_SIZE / sizeof(uint16_t)] = {0};

        uint32_t ints = save_and_disable_interrupts();
        flash_range_erase(FLASH_SECTOR_OFFSET, FLASH_SECTOR_SIZE);
        flash_range_program(FLASH_SECTOR_OFFSET, (uint8_t *)sector_buffer, FLASH_PAGE_SIZE);
        restore_interrupts(ints);
    }
}

void __not_in_flash_func(checkThresholdContent)()
{
    uint16_t *th_ptr = (uint16_t *)(XIP_BASE + FLASH_THRESHOLD_INFO_OFFSET);

    // Threshold value control
    if (th_ptr[0] == 0xFFFF)
    {
        PRINTF("threshold value is empty, setting to 5 as default...\n");

        uint16_t th_arr[256 / sizeof(uint16_t)] = {0};
        th_arr[0] = vrms_threshold;
        th_arr[1] = th_ptr[1];

        uint32_t ints = save_and_disable_interrupts();
        flash_range_erase(FLASH_THRESHOLD_INFO_OFFSET, FLASH_SECTOR_SIZE);
        flash_range_program(FLASH_THRESHOLD_INFO_OFFSET, (uint8_t *)th_arr, FLASH_PAGE_SIZE);
        restore_interrupts(ints);
    }
    // Threshold Records Sector control
    if (th_ptr[1] == 0xFFFF)
    {
        PRINTF("threshold record's sector value is empty, setting to 0 as default...\n");

        uint16_t th_arr[256 / sizeof(uint16_t)] = {0};
        th_arr[0] = th_ptr[0];
        th_arr[1] = 0;

        uint32_t ints = save_and_disable_interrupts();
        flash_range_erase(FLASH_THRESHOLD_INFO_OFFSET, FLASH_SECTOR_SIZE);
        flash_range_program(FLASH_THRESHOLD_INFO_OFFSET, (uint8_t *)th_arr, FLASH_PAGE_SIZE);
        restore_interrupts(ints);
    }
}

void __not_in_flash_func(updateThresholdSector)(uint16_t sector_val)
{
    uint16_t th_arr[256 / sizeof(uint16_t)] = {0};

    th_arr[0] = getVRMSThresholdValue();
    th_arr[1] = sector_val;

    if (xSemaphoreTake(xFlashMutex, portMAX_DELAY) == pdTRUE)
    {
        PRINTF("UPDATETHRESHOLDSECTOR: write flash mutex received\n");
        flash_range_erase(FLASH_THRESHOLD_INFO_OFFSET, FLASH_SECTOR_SIZE);
        flash_range_program(FLASH_THRESHOLD_INFO_OFFSET, (uint8_t *)th_arr, FLASH_PAGE_SIZE);
        xSemaphoreGive(xFlashMutex);
    }
    else
    {
        PRINTF("MUTEX CANNOT RECEIVED!\n");
    }
}

#if WITHOUT_BOOTLOADER
// This function adds serial number to flash area
void __not_in_flash_func(addSerialNumber)()
{
    PRINTF("ADDSERIALNUMBER: entered addserialnumber function.\n");

    uint32_t ints = save_and_disable_interrupts();
    uint8_t *snumber = (uint8_t *)(XIP_BASE + FLASH_SERIAL_OFFSET);

    if (snumber[0] == 0xFF)
    {
        PRINTF("ADDSERIALNUMBER: serial number is going to be added.\n");

        flash_range_erase(FLASH_SERIAL_OFFSET, FLASH_SECTOR_SIZE);
        flash_range_program(FLASH_SERIAL_OFFSET, (const uint8_t *)s_number, FLASH_PAGE_SIZE);
    }

    restore_interrupts(ints);
}
#endif

void __not_in_flash_func(setProgramStartDate)(datetime_t *ct)
{
    uint8_t *flash_reset_count_offset = (uint8_t *)(XIP_BASE + FLASH_RESET_COUNT_OFFSET);
    uint8_t current_time_buffer[16] = {0};
    uint8_t flash_reset_count_buffer[FLASH_SECTOR_SIZE] = {0};
    uint16_t offset = 0;

    memcpy(flash_reset_count_buffer, flash_reset_count_offset, FLASH_SECTOR_SIZE);

    setDateToCharArray(ct->year, (char *)current_time_buffer);
    setDateToCharArray(ct->month, (char *)current_time_buffer + 2);
    setDateToCharArray(ct->day, (char *)current_time_buffer + 4);
    setDateToCharArray(ct->hour, (char *)current_time_buffer + 6);
    setDateToCharArray(ct->min, (char *)current_time_buffer + 8);
    setDateToCharArray(ct->sec, (char *)current_time_buffer + 10);

    current_time_buffer[12] = 0x7F;
    current_time_buffer[13] = 0x7F;
    current_time_buffer[14] = 0x7F;
    current_time_buffer[15] = 0x7F;

    PRINTF("SETPROGRAMSTARTDATE: Program start date is set to: ");
    printBufferHex(current_time_buffer, 10);
    PRINTF("\n");

    for (uint16_t i = 0; i < FLASH_SECTOR_SIZE; i += 16)
    {
        if (flash_reset_count_buffer[offset] == 0xFF || flash_reset_count_offset[offset] == 0x00)
        {
            break;
        }

        offset += 16;
    }

    if (offset >= FLASH_SECTOR_SIZE)
    {
        memset(flash_reset_count_buffer, 0, FLASH_SECTOR_SIZE);
        offset = 0;
    }

    memcpy(flash_reset_count_buffer + offset, current_time_buffer, sizeof(current_time_buffer));

    uint32_t ints = save_and_disable_interrupts();
    flash_range_erase(FLASH_RESET_COUNT_OFFSET, FLASH_SECTOR_SIZE);
    flash_range_program(FLASH_RESET_COUNT_OFFSET, flash_reset_count_buffer, FLASH_SECTOR_SIZE);
    restore_interrupts(ints);

    printBufferHex(flash_reset_count_offset, FLASH_PAGE_SIZE);
}

void __not_in_flash_func(writeSuddenAmplitudeChangeRecordToFlash)(struct AmplitudeChangeTimerCallbackParameters *ac_params)
{
    PRINTF("write sudden amplitude change record to flash\r\n");

    uint16_t ac_sector = 0;
    uint8_t *flash_ac_records = (uint8_t *)(XIP_BASE + FLASH_AMPLITUDE_CHANGE_OFFSET);
    size_t sector_count = 0;
    // amplitude change data buffer
    struct AmplitudeChangeData ac_flash_data;

    if (xSemaphoreTake(xFlashMutex, portMAX_DELAY) == pdTRUE)
    {
        PRINTF("WRITESUDDENAMPCHANGE: loop mutex received\r\n");
        for (sector_count = 0; sector_count < FLASH_AMPLITUDE_RECORDS_TOTAL_SECTOR * FLASH_SECTOR_SIZE; sector_count += FLASH_SECTOR_SIZE)
        {
            if (flash_ac_records[sector_count] == 0xFF)
            {
                break;
            }
            else
            {
                ac_sector++;
            }
        }

        xSemaphoreGive(xFlashMutex);
    }

    if (ac_sector == FLASH_AMPLITUDE_RECORDS_TOTAL_SECTOR)
    {
        ac_sector = 0;
    }

    PRINTF("ac sector = %d\r\n", ac_sector);

    // set current date
    setDateToCharArray(current_time.year, ac_flash_data.year);
    setDateToCharArray(current_time.month, ac_flash_data.month);
    setDateToCharArray(current_time.day, ac_flash_data.day);
    setDateToCharArray(current_time.hour, ac_flash_data.hour);
    setDateToCharArray(current_time.min, ac_flash_data.min);
    setDateToCharArray(current_time.sec, ac_flash_data.sec);

    // set samples
    if (xSemaphoreTake(xFIFOMutex, portMAX_DELAY) == pdTRUE)
    {
        uint16_t copy_buffer[ADC_FIFO_SIZE] = {0};
        getLastNElementsToBuffer(&adc_fifo, copy_buffer, ADC_FIFO_SIZE);
        // memcpy(ac_flash_data.sample_buffer, sample_buffer, ac_params->adc_fifo_size * sizeof(uint16_t));
        for (uint16_t i = 0; i < ADC_FIFO_SIZE; i++)
        {
            ac_flash_data.sample_buffer[i] = (copy_buffer[i] >> 4) & 0xFF;
        }
        xSemaphoreGive(xFIFOMutex);
    }

    // set vrms values
    memcpy(ac_flash_data.vrms_values_buffer, ac_params->vrms_values_buffer, ac_params->vrms_values_buffer_size_bytes);

    // set variance
    ac_flash_data.variance = ac_params->variance;

    // set padding
    memset(ac_flash_data.padding, 0, sizeof(ac_flash_data.padding));

    if (xSemaphoreTake(xFlashMutex, portMAX_DELAY) == pdTRUE)
    {
        PRINTF("WRITESUDDENAMPCHANGE: write flash mutex received\r\n");
        if (ac_sector == (FLASH_AMPLITUDE_RECORDS_TOTAL_SECTOR - 1))
        {
            flash_range_erase(FLASH_AMPLITUDE_CHANGE_OFFSET + (ac_sector * FLASH_SECTOR_SIZE), FLASH_SECTOR_SIZE);
        }
        else
        {
            flash_range_erase(FLASH_AMPLITUDE_CHANGE_OFFSET + (ac_sector * FLASH_SECTOR_SIZE), 2 * FLASH_SECTOR_SIZE);
        }
        flash_range_program(FLASH_AMPLITUDE_CHANGE_OFFSET + (ac_sector * FLASH_SECTOR_SIZE), (const uint8_t *)&ac_flash_data, FLASH_SECTOR_SIZE);
        xSemaphoreGive(xFlashMutex);
    }
    else
    {
        PRINTF("MUTEX CANNOT RECEIVED!\r\n");
    }

    PRINTF("Written ac sector: %d\r\n", ac_sector);
}

#endif
