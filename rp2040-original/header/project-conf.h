// Device Password (will be written to flash)
#define DEVICE_PASSWORD "12345678"
// Device software version number
#define SOFTWARE_VERSION "V1.0.0"
// production date of device (yy-mm-dd)
#define PRODUCTION_DATE "24-03-13"
// Debugs
#define DEBUG 0
// bootloader select
#define WITHOUT_BOOTLOADER 0
// vrms multiplier value
#define VRMS_MULTIPLICATION_VALUE 150
// watchdog timeout ms to reset device
#define WATCHDOG_TIMEOUT_MS 5000
// DEBUG MACRO
#if DEBUG
#define PRINTF(x, ...) printf(x, ##__VA_ARGS__)
#else
#define PRINTF(x, ...)
#endif
