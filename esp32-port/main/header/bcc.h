#ifndef BCC_H
#define BCC_H

#include <stdint.h>
#include <stdbool.h>

// dev branch'teki blink/header/bcc.h ile birebir ayni - donanimdan bagimsiz,
// degisiklik gerekmedi.
void bccGenerate(uint8_t *data_buffer, uint8_t size, uint8_t *xor);
bool bccControl(uint8_t *buffer, uint8_t size);
void sendErrorMessage(char *error_text);
void led_blink_pattern(int pattern_id, bool once);
#endif
