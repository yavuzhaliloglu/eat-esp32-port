#ifndef RTC_H
#define RTC_H

#include "defines.h"
#include "variables.h"

uint8_t initI2C()
{
    uint i2c_brate = 0;
    i2c_brate = i2c_init(i2c0, 400 * 1000);
    if (i2c_brate == 0)
    {
        PRINTF("I2C INIT ERROR!\n");
        return 0;
    }
    return 1;
}

// This function converts decimal value to BCD value
uint8_t decimalToBCD(uint8_t decimalValue)
{
    return ((decimalValue / 10) << 4) | (decimalValue % 10);
}

// This function converts BCD value to decimal value
uint8_t bcd_to_decimal(uint8_t bcd)
{
    return bcd - 6 * (bcd >> 4);
}

// This function sets the PT7C4338's Real Time
uint8_t setTimePt7c4338(struct i2c_inst *i2c, uint8_t address, uint8_t seconds, uint8_t minutes, uint8_t hours, uint8_t day, uint8_t date, uint8_t month, uint8_t year)
{
    uint8_t buf[8];
    buf[0] = PT7C4338_REG_SECONDS;
    buf[1] = decimalToBCD(seconds);
    buf[2] = decimalToBCD(minutes);
    buf[3] = decimalToBCD(hours);
    buf[4] = decimalToBCD(day);
    buf[5] = decimalToBCD(date);
    buf[6] = decimalToBCD(month);
    buf[7] = decimalToBCD(year);

    if (i2c_write_blocking(i2c, address, buf, 8, false) != 8)
    {
        PRINTF("SETTIMEPT7C: I2C WRITE ERROR!\n");
        return 0;
    }

    return 1;
}

// This function gets the PT7C4338's Real Time and sets it to datetime object
uint8_t getTimePt7c4338(datetime_t *dt)
{
    uint8_t buffer[7] = {PT7C4338_REG_SECONDS};

    ;
    if (i2c_write_blocking(I2C_PORT, I2C_ADDRESS, buffer, 1, 1) != 1)
    {
        PRINTF("GETTIMEPT7C: I2C WRITE ERROR!\n");
        return 0;
    }

    if (i2c_read_blocking(I2C_PORT, I2C_ADDRESS, buffer, 7, 0) != 7)
    {
        PRINTF("GETTIMEPT7C: I2C READ ERROR!\n");
        return 0;
    }

    dt->year = bcd_to_decimal(buffer[6]);
    dt->month = bcd_to_decimal(buffer[5]);
    dt->day = bcd_to_decimal(buffer[4]);
    dt->dotw = bcd_to_decimal(buffer[3]);
    dt->hour = bcd_to_decimal(buffer[2]);
    dt->min = bcd_to_decimal(buffer[1]);
    dt->sec = bcd_to_decimal(buffer[0]);

    return 1;
}

#endif
