#include "header/project_globals.h"

// dev branch'teki blink/src/globals.c dosyasindan uyarlanmistir.
// Bu dosya, project_globals.h'de "extern" olarak bildirilen degiskenlerin
// GERCEK TANIMLARINI (bellek ayrilan hallerini) icerir - baska hicbir .c
// dosyasi bu degiskenleri (mesela xFIFOMutex) kullanamaz, onlar burada
// gercekten var olmadan. Mantik ve baslangic degerleri BIREBIR AYNI.

// =============================================================================
// GLOBAL DEGISKENLER
// =============================================================================

// ADC DEGISKENLERI
ADC_FIFO adc_fifo;
uint8_t load_profile_record_period = 15;
volatile float vrms_max_last = 0.0;
volatile float vrms_min_last = 0.0;
volatile float vrms_mean_last = 0.0;
volatile float vrms_instant = 0.0;
uint16_t vrms_threshold = 5;
uint8_t threshold_set_before = 0;
// ⚠️ bias_voltage KALDIRILDI: self-referencing RMS yontemine gecince hic
// yazilmiyordu (her zaman 0 kalirdi), kullanicinin/hocanin istegiyle
// tamamen cikarildi.
// Baslangic degeri VRMS_MULTIPLICATION_VALUE define'indan (project_conf.h,
// 148.8f) - artik BLE'den calisma-zamaninda degistirilebilir bir degisken.
float vrms_multiplication_value = VRMS_MULTIPLICATION_VALUE;

// UART DEGISKENLERI
bool password_correct_flag = false;

// FLASH DEGISKENLERI
// serial_number global'i KALDIRILDI - seri no artik flash'tan okunmuyor,
// dogrudan DEVICE_SERIAL_NUMBER makrosundan geliyor (bkz. spiflash.c).
uint16_t sector_data = 0;
uint16_t th_sector_data = 0;
struct FlashData flash_data[FLASH_SECTOR_SIZE / sizeof(struct FlashData)] = {0};
struct ThresholdData th_flash_buf[FLASH_SECTOR_SIZE / sizeof(struct ThresholdData)] = {0};

// RTC DEGISKENLERI
char datetime_buffer[64];
char *datetime_str = &datetime_buffer[0];
datetime_t current_time = {
    .year = 2026,
    .month = 1,
    .day = 1,
    .dotw = 4,
    .hour = 0,
    .min = 0,
    .sec = 0};

// FreeRTOS TASK HANDLE'LARI
TaskHandle_t xADCHandle;
TaskHandle_t xADCSampleHandle;
TaskHandle_t xUARTHandle;
TaskHandle_t xResetHandle;
TaskHandle_t xGetRTCHandle;
TaskHandle_t xStatusLedHandle;
TaskHandle_t xWatchdogHandle;

SemaphoreHandle_t xFlashMutex;
SemaphoreHandle_t xFIFOMutex;
SemaphoreHandle_t xVRMSLastValuesMutex;
SemaphoreHandle_t xVRMSThresholdMutex;
SemaphoreHandle_t xThresholdSetFlagMutex;

// Bosta "cihaz calisiyor" gostergesi: Pico'daki gibi 1 saniye yanik,
// 1 saniye sonuk. Degerler milisaniye; tam cevrim 2 saniye (0.5 Hz).
// ⚠️ Dizi uzunlugunu degistirirsen asagidaki patterns[] icindeki sayiyi da
// guncelle.
const uint16_t pattern_idle[] = {1000, 1000};

// Hata desenleri: milisaniye cinsinden yanik/sonuk sureleri.
// Pico sayaci 2 ms'de bir arttigi icin oradaki dizi degerleri burada
// iki kat milisaniye olarak kullanilir; boylece tum desenlerin ritmi aynidir.
const uint16_t led_pattern_uart_not_readable[] = {100, 1900};                        // 1 Kisa
const uint16_t led_pattern_message_timeout[] = {500, 1500};                          // 1 Uzun
const uint16_t led_pattern_invalid_request_mode[] = {100, 200, 100, 1600};           // 2 Kisa
const uint16_t led_pattern_invalid_serial_number[] = {500, 200, 500, 800};           // 2 Uzun
const uint16_t led_pattern_flash_mutex_not_taken[] = {100, 200, 100, 200, 100, 1300}; // 3 Kisa
const uint16_t led_pattern_fifo_mutex_not_taken[] = {50, 100, 50, 1800};              // Kalp atisi (2 hizli)
const uint16_t led_pattern_vrms_values_mutex_not_taken[] = {100, 200, 500, 1200};     // Kisa-Uzun
const uint16_t led_pattern_vrms_threshold_mutex_not_taken[] = {500, 200, 100, 1200};  // Uzun-Kisa
const uint16_t led_pattern_threshold_set_mutex_not_taken[] = {50, 50, 50, 50, 50, 50, 50, 50, 50, 1550}; // 5 hizli
const uint16_t led_pattern_rx_buffer_overflow_isr[] = {100, 100, 100, 100, 100, 100, 500, 900};           // 3 hizli, 1 uzun
const uint16_t led_pattern_stackoverflow[] = {1000, 400, 200, 1400};
const uint16_t led_pattern_flash_metadata_corrupt[] = {500, 200, 500, 200, 100, 1300}; // 2 Uzun, 1 Kisa
const uint16_t led_pattern_rtc_stalled[] = {1000, 200, 1000, 200, 1000, 600};           // 3 Uzun (saat durdu)

const LedPattern patterns[] = {
    {pattern_idle, 2},
    {led_pattern_uart_not_readable, 2},
    {led_pattern_message_timeout, 2},
    {led_pattern_invalid_request_mode, 4},
    {led_pattern_invalid_serial_number, 4},
    {led_pattern_flash_mutex_not_taken, 6},
    {led_pattern_fifo_mutex_not_taken, 4},
    {led_pattern_vrms_values_mutex_not_taken, 4},
    {led_pattern_vrms_threshold_mutex_not_taken, 4},
    {led_pattern_threshold_set_mutex_not_taken, 10},
    {led_pattern_rx_buffer_overflow_isr, 8},
    {led_pattern_stackoverflow, 4},
    {led_pattern_flash_metadata_corrupt, 6},
    {led_pattern_rtc_stalled, 6}};

// led_blink_pattern() sinirini buradan alir; elle yazilmis bir sayi kalirsa
// yeni desen eklendiginde sessizce calismaz, silindiginde dizi disina tasar.
const uint8_t patterns_count = sizeof(patterns) / sizeof(patterns[0]);

// Watchdog degiskeni
volatile uint32_t task_health_flags = 0;
