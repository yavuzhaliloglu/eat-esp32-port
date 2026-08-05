#include "header/rtc.h"
#include "driver/i2c_master.h"
#include "header/project_globals.h"

// dev branch'teki blink/src/rtc.c dosyasindan uyarlanmistir.
// BCD cevirme mantigi (decimalToBCD/bcd_to_decimal) ve register haritasi
// (RTC_REG_SECONDS'tan itibaren 7 byte: sn/dk/sa/haftagunu/tarih/ay/yil)
// BIREBIR AYNI - i2c_rtc_test projesinde fiziksel olarak dogrulanmisti
// (Asama 2 Madde 3, RTC cipi 0x68 adresinde bulunmustu).
//
// FARK: Pico SDK'nin i2c_write_blocking()/i2c_read_blocking() (global i2c0
// pointer + her cagrida adres parametresi) yerine ESP-IDF'in yeni nesil
// i2c_master API'si kullaniliyor - bus ve cihaz once "handle" olarak
// olusturuluyor (initI2C icinde), sonraki her okuma/yazma o handle
// uzerinden yapiliyor.

static i2c_master_bus_handle_t s_i2c_bus;
static i2c_master_dev_handle_t s_rtc_dev;

uint8_t initI2C()
{
    i2c_master_bus_config_t bus_config = {
        .i2c_port = -1,
        .sda_io_num = RTC_I2C_SDA_PIN,
        .scl_io_num = RTC_I2C_SCL_PIN,
        .clk_source = I2C_CLK_SRC_DEFAULT,
        .glitch_ignore_cnt = 7,
        .flags.enable_internal_pullup = true,
    };
    if (i2c_new_master_bus(&bus_config, &s_i2c_bus) != ESP_OK)
    {
        PRINTF("I2C INIT ERROR!\n");
        return 0;
    }

    i2c_device_config_t dev_config = {
        .dev_addr_length = I2C_ADDR_BIT_LEN_7,
        .device_address = RTC_I2C_ADDRESS,
        .scl_speed_hz = 100000,
    };
    if (i2c_master_bus_add_device(s_i2c_bus, &dev_config, &s_rtc_dev) != ESP_OK)
    {
        PRINTF("I2C RTC CIHAZI EKLENEMEDI!\n");
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
uint8_t setTimePt7c4338(uint8_t seconds, uint8_t minutes, uint8_t hours, uint8_t day, uint8_t date, uint8_t month, uint8_t year)
{
    uint8_t buf[8];
    buf[0] = RTC_REG_SECONDS;
    buf[1] = decimalToBCD(seconds);
    buf[2] = decimalToBCD(minutes);
    buf[3] = decimalToBCD(hours);
    buf[4] = decimalToBCD(day);
    buf[5] = decimalToBCD(date);
    buf[6] = decimalToBCD(month);
    buf[7] = decimalToBCD(year);

    if (i2c_master_transmit(s_rtc_dev, buf, sizeof(buf), 1000) != ESP_OK)
    {
        PRINTF("SETTIMEPT7C: I2C WRITE ERROR!\n");
        return 0;
    }

    return 1;
}

// This function gets the PT7C4338's Real Time and sets it to datetime object
uint8_t getTimePt7c4338(datetime_t *dt)
{
    uint8_t reg = RTC_REG_SECONDS;
    uint8_t buffer[7];

    if (i2c_master_transmit_receive(s_rtc_dev, &reg, 1, buffer, sizeof(buffer), 1000) != ESP_OK)
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
