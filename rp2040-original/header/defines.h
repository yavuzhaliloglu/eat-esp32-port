#ifndef DEFINES_H
#define DEFINES_H

//    36kb                236kB 256kB               504kB 512kB                                                         380 Sectors                                                                    2048kB
// |-----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------|
// |   |                    | | |                      | | | |                                                                                     |                                                     |
// | B |      Main Program  |X|T|      OTA Program     |R|N|S|                                  Records                                            |           Sudden Amplitude Change Records           |
// |   |                    | | |                      | | | |                                                                                     |                                                     |
// |-----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------|
//                          240kB                      508kB 516kB                              280 Sectors                                                             100 Sectors
//  B -> Bootloader (26kB)
//  X -> Threshold Variable and Sector Contents
//  T -> Threshold Contents
//  R -> Reset Count Contents
//  N -> Serial Number Contents
//  S -> Sector Contents

// MESSAGES THAT THIS DEVICE ACCEPTS

// Request Message Without Serial Number and Flag:  /?!\r\n                                                     -> Length: 5
// Request Message Without Serial Number:           /?ALP!\r\n                                                  -> Length: 8
// Request Message with Serial Number and Flag:     /?ALP612400001!\r\n                                         -> Length: 17

// Acknowledgement Message:                         [ACK]0ZX\r\n                                                -> Length: 6

// OTA Request:                                     [SOH]W2[STX]O.T.A()[ETX][BCC]                               -> Length: 13

// Password:                                        [SOH]P1[STX](12345678)[ETX][BCC]                            -> Length: 16
// Load Profile with Date:                          [SOH]R2[STX]P.01(24-07-13,13:00;24-07-14,14:00)[ETX][BCC]   -> Length: 41
// Load Profile without Date:                       [SOH]R2[STX]P.01(;)[ETX][BCC]                               -> Length: 13
// Time Set:                                        [SOH]W2[STX]0.9.1(13:00:00)[ETX][BCC]                       -> Length: 21
// Date Set:                                        [SOH]W2[STX]0.9.2(24-07-15)[ETX][BCC]                       -> Length: 21
// Production Information:                          [SOH]R2[STX]96.1.3()[ETX][BCC]                              -> Length: 14
// Set Threshold Value:                             [SOH]W2[STX]T.V.1(000)[ETX][BCC]                            -> Length: 16
// Get Threshold Value:                             [SOH]R2[STX]T.R.1()[ETX][BCC]                               -> Length: 13
// Set Threshold PIN:                               [SOH]W2[STX]T.P.1()[ETX][BCC]                               -> Length: 13
// Get Sudden Amplitude Change Records              [SOH]R2[STX]9.9.0()[ETX][BCC]                               -> Length: 13
// Read Current Time                                [SOH]R2[STX]0.9.1()[ETX][BCC]                               -> Length: 13
// Read Current Date                                [SOH]R2[STX]0.9.2()[ETX][BCC]                               -> Length: 13
// Read Serial Number                               [SOH]R2[STX]0.0.0()[ETX][BCC]                               -> Length: 13
// Read Last VRMS Max                               [SOH]R2[STX]32.7.0()[ETX][BCC]                              -> Length: 14
// Read Last VRMS Min                               [SOH]R2[STX]52.7.0()[ETX][BCC]                              -> Length: 14
// Read Last VRMS Mean                              [SOH]R2[STX]72.7.0()[ETX][BCC]                              -> Length: 14
// Read Reset Dates                                 [SOH]R2[STX]R.D.0()[ETX][BCC]                               -> Length: 13
// End Connection:                                  [SOH]B0[ETX]q                                               -> Length: 5

// FLASH DEFINES

// this is the start offset of the program
#define FLASH_PROGRAM_OFFSET 36 * 1024
// threshold values offset (first 2 byte value is threshold value, second 2 byte value is threshold records sector value)
#define FLASH_THRESHOLD_INFO_OFFSET (256 * 1024) - (5 * FLASH_SECTOR_SIZE)
// threshold values offset
#define FLASH_THRESHOLD_OFFSET (256 * 1024) - (4 * FLASH_SECTOR_SIZE)
// this is the start offset of OTA program will written
#define FLASH_REPROGRAM_OFFSET 256 * 1024
// reset count offset
#define FLASH_RESET_COUNT_OFFSET (512 * 1024) - (2 * FLASH_SECTOR_SIZE)
// this is the start offset of device serial number information
#define FLASH_SERIAL_OFFSET (512 * 1024) - FLASH_SECTOR_SIZE
// this is the start offset of sector information that load profile records will written
#define FLASH_SECTOR_OFFSET 512 * 1024
// this is the start offset of load profile records will written
#define FLASH_DATA_OFFSET (512 * 1024) + FLASH_SECTOR_SIZE
// amplitude change records offset
#define FLASH_AMPLITUDE_CHANGE_OFFSET FLASH_DATA_OFFSET + (FLASH_TOTAL_SECTORS * FLASH_SECTOR_SIZE)
// repgrogram area size
#define FLASH_REPROGRAM_SIZE FLASH_RESET_COUNT_OFFSET - FLASH_REPROGRAM_OFFSET
// this is the size of OTA program block will written to flash. it has to be multiple size of flash area.
#define FLASH_RPB_BLOCK_SIZE 7 * FLASH_PAGE_SIZE
// this is the count of total sectors in flash expect first 512kB + 4kB of flash (main program(256kB), OTA program(256kB), sector information(4kB))
#define FLASH_TOTAL_SECTORS 280
// amplitude records size as sectors
#define FLASH_AMPLITUDE_RECORDS_TOTAL_SECTOR 100
// this is the size of one load profile record
#define FLASH_RECORD_SIZE 16
// this is the count of total records can be kept in a flash
#define FLASH_TOTAL_RECORDS (PICO_FLASH_SIZE_BYTES - (FLASH_DATA_OFFSET)) / FLASH_RECORD_SIZE
// serial number size
#define SERIAL_NUMBER_SIZE 9

// UART DEFINES

// MD5 checksum length
#define MD5_DIGEST_LENGTH 16
// This is the required number for the watchdog timer.
#define ENTRY_MAGIC 0xb105f00d
// UART ID for communication
#define UART0_ID uart0
// Baud Rate for UART. 300 bits per second.
#define BAUD_RATE 300
// Those are TX (Transmit) and RX (Receiver) pins for UART
#define UART0_TX_PIN 0
#define UART0_RX_PIN 1
// Data format to send UART includes 7 data bit (ASCII)
#define DATA_BITS 7
// 1 Stop bit for transfer format
#define STOP_BITS 1
// Even parity bit for transfer format (Check 1's in data)
#define PARITY UART_PARITY_EVEN
// UART Task priority for the FreeRTOS Kernel
#define UART_TASK_PRIORITY 3
// Stack size for UART Task
#define UART_TASK_STACK_SIZE (1024 * 3)

// POWER LED
#define POWER_LED_PIN 18

// RESET DEFINES

// Reset Pin Select
#define RESET_PULSE_PIN 2
// Standby Time for the Task
#define INTERVAL_MS 60000

// THRESHOLD PIN DEFINE
#define THRESHOLD_PIN 17

// ADC DEFINES

#define ADC_FIFO_SIZE 4000
// samples to collect from ADC Pin
#define VRMS_SAMPLE_SIZE 2000
// sample size per vrms calculation
#define SAMPLE_SIZE_PER_VRMS_CALC 200
// VRMS buffer size to calculate min, max and mean values and write to flash
#define VRMS_BUFFER_SIZE 900
// ADC Voltage Pin
#define ADC_READ_PIN 26
// ADC BIAS Voltage Pin
#define ADC_BIAS_PIN 27
// ADC Voltage Input
#define ADC_VRMS_SAMPLE_INPUT 0
// ADC BIAS Input
#define ADC_BIAS_INPUT 1
// BIAS Sample count
#define BIAS_SAMPLE_SIZE 2000
// amplitude threshold
#define AMPLITUDE_THRESHOLD 5
// mean calculation window size
#define MEAN_CALCULATION_WINDOW_SIZE 20
// mean calculation shifting size
#define MEAN_CALCULATION_SHIFT_SIZE 5

// RTC DEFINES

#define PT7C4338_REG_SECONDS 0x00
// I2C Port Select
#define I2C_PORT i2c0
// I2C Address
#define I2C_ADDRESS 0x68
// SDA and SCL pins for I2C
#define RTC_I2C_SDA_PIN 20
#define RTC_I2C_SCL_PIN 21

// SPECIAL CHARACTERS

#define SOH 0x01
#define STX 0x02
#define ETX 0x03
#define ACK 0x06
#define NACK 0x15

#endif
