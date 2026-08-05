#ifndef PRINT_H
#define PRINT_H

#include <stdint.h>
#include <stddef.h>

// dev branch'teki blink/header/print.h ile birebir ayni - donanimdan
// bagimsiz, degisiklik gerekmedi.

// This function prints a 1 byte value as binary
void printBinaryHex(uint8_t value);
// This function prints a buffer as hexadecimal values
void printBufferHex(uint8_t *buf, size_t len);
// This function prints a buffer as hexadecimal values
void printBufferUint16T(uint16_t *buf, size_t len);
// This function prints a buffer as hexadecimal values
void printBufferFloat(float *buf, size_t len);

#endif
