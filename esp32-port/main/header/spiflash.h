#ifndef SPIFLASH_H
#define SPIFLASH_H

#include <stdint.h>
#include <stddef.h>
#include "header/project_globals.h"

// dev branch'teki blink/header/spiflash.h dosyasindan uyarlanmistir.
//
// DEV'DEN BILEREK CIKARILANLAR:
// - read_sr / send_write_enable_command / send_write_protect_command /
//   read_flash_status_registers: RP2040'in kendi flash cipine dusuk seviye
//   SPI NOR komutlari (yaz-korumasi ac/kapa vs.) gonderiyordu. ESP-IDF'in
//   esp_partition API'si bunu kendi icinde hallediyor, manuel yapmamiza
//   gerek yok.
//
// ⚠️ DUZELTME: get_record_indexes/add_date_to_buffer/parse_load_profile_dates
// ONCE "olu kod" sanilmisti - YANLISTI (o not master branch icin gecerliydi).
// dev'in gercek uart.c'sinde {"P.01", Reading, CMD_TYPE_ANY} var, yani bu
// mekanizma GERCEKTEN kullaniliyor. Hepsi SADIK sekilde portlandi (spiflash.c
// icinde static/internal fonksiyonlar olarak, bu header'da yoklar cunku
// dev'de de public API degillerdi).
//
// ⚠️ writeSuddenAmplitudeChangeRecordToFlash: dev'de sadece DECLARE edilmis,
// implementasyonu HICBIR dosyada yok (CONF_SUDDEN_AMPLITUDE_CHANGE_ENABLED=0
// oldugu icin hic fark edilmemis eksik). Bizde de ayni sekilde declare
// edilip implement EDILMEDI - portlanacak gercek bir kod yok.
//
// PORT ASAMALARI (hepsi tamamlandi, hepsi birlikte test edilecek):
// [x] Asama 1: saf mantik fonksiyonlari (donanima dokunmayan)
// [x] Asama 2: gercek flash okuma/yazma
// [x] Asama 3: load profile / sektor kontrol fonksiyonlari
// [x] Asama 4: seri no / reset kaydi (ani degisim kaydi haric, yukarida aciklandi)

// --- ASAMA 1: saf mantik ---
// This function converts the datetime value to char array as characters to write flash correctly
void setDateToCharArray(int value, char *array);
// this function converts a float value's floating value to uint8_t value.
uint8_t floatDecimalDigitToUint8t(float float_value);
// This function gets a buffer which includes VRMS values, and calculate the max, min and mean values of this buffer and sets the variables.
VRMS_VALUES_RECORD vrmsSetMinMaxMean(float *buffer, uint16_t size);
float convertVRMSValueToFloat(uint8_t value, uint8_t value_dec);
// this function converts an array to datetime value
void arrayToDatetime(datetime_t *dt, uint8_t *arr);
// This functon compares two datetime values and return an int value
int datetimeComp(datetime_t *dt1, datetime_t *dt2);
// This function copies a datetime value to another datetime value
void datetimeCopy(datetime_t *src, datetime_t *dst);

// --- ASAMA 2: gercek flash okuma/yazma ---
// This function gets the contents like sector data, last records contents from flash and sets them to variables.
void getFlashContents();
// This function writes current sector data to flash.
void setSectorData(uint16_t sector_value);
// This function sets the current time values which are 16 bytes total and calculated VRMS values to flash
void setFlashData(VRMS_VALUES_RECORD *vrms_values);
// This function writes flash_data content to flash area
void SPIWriteToFlash(VRMS_VALUES_RECORD *vrms_values);

// --- ASAMA 3: load profile / sektor kontrol ---
// This function searches the requested data in flash by starting from flash record beginning offset, collects data from flash and sends it to UART to show load profile content
void send_load_profile_records(uint8_t *buf);
void checkSectorContent();
void checkThresholdContent();
void updateThresholdSector(uint16_t sector_val);
// th_flash_buf'in tamamini (bir sektor) "threshold_rec" partition'ina yazar -
// dev'deki adc.c/writeThresholdRecord()'un dogrudan yaptigi flash yazma
// adiminin ESP32 karsiligi, adc.c'den cagrilir.
void writeThresholdRecordsSectorToFlash(uint16_t sector_val);

// --- ASAMA 4: reset kaydi / ani degisim kaydi ---
// ⚠️ addSerialNumber() KALDIRILDI - seri no artik flash'a yazilmiyor, dogrudan
// DEVICE_SERIAL_NUMBER makrosundan geliyor (bkz. spiflash.c'deki ayrintili not).
void setProgramStartDate(datetime_t *ct);
void writeSuddenAmplitudeChangeRecordToFlash(struct AmplitudeChangeTimerCallbackParameters *ac_params);

// --- YENI (BLE tarih-aralikli sorgu icin, kullanicinin istegiyle eklendi):
// send_load_profile_records() ile AYNI arama/filtreleme mantigini kullanir
// (get_record_indexes/is_record_between_date_values - internal/static
// fonksiyonlar, spiflash.c icinde kaliyor), ama RS485/BCC yerine duz bir
// metin tamponuna yaziyor. ---
// dt_start/dt_end araligindaki load profile kayitlarini
// "tarih,saat,min,max,ortalama;..." formatinda out_buf'a yazar.
void getLoadProfileRecordsAsText(datetime_t *dt_start, datetime_t *dt_end, char *out_buf, size_t out_buf_size);
// flash'taki TUM load profile kayitlarinin FARKLI (distinct) tarihlerini
// "YY-MM-DD,YY-MM-DD,..." formatinda out_buf'a yazar - takvimde hangi
// gunlerin secilebilir/aktif oldugunu gostermek icin.
void getLoadProfileAvailableDates(char *out_buf, size_t out_buf_size);

#endif
