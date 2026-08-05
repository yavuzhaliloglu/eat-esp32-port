#ifndef BCC_H
#define BCC_H

// Generate BCC for a buffer.
void bccGenerate(uint8_t *data_buffer, uint8_t size, uint8_t *xor)
{
    for (uint8_t i = 0; i < size; i++)
        *xor ^= data_buffer[i];

    PRINTF("BCCGENERATE: returned xor result is: %02X\n", *xor);
}

// Generate a BCC for a buffer which starts with SOH character and compare with buffer's last character, which is BCC of buffer.
bool bccControl(uint8_t *buffer, uint8_t size)
{
    uint8_t xor_result = SOH;

    for (uint8_t i = 0; i < size - 1; i++)
        xor_result ^= buffer[i];

    PRINTF("BCCCONTROL: generated xor result is: %02X\n", xor_result);
    PRINTF("BCCCONTROL: xor result in coming message is: %02X\n", buffer[size - 1]);

    return xor_result == buffer[size - 1];
}

// Generate a BCC and add it to end of a buffer. Buffer size should be 1 byte more than the buffer size.
void setBCC(uint8_t *buffer, uint8_t size, uint8_t xor)
{
    for (int i = 0; i < size; i++)
        xor ^= buffer[i];

    PRINTF("SETBCC: generated xor result is: %02X\n", xor);

    buffer[size] = xor;
}

// This function sends error message
void sendErrorMessage(char *error_text)
{
    // error message should be max 35 characters. 34 characters are message and last 1 character is BCC
    char error_message[34] = {0};
    uint8_t error_message_xor = STX; // set to 0x02 due to ignore STX character

    if (strlen(error_text) > 30)
    {
        PRINTF("SENDERRORMESSAGE: Error message is too long! Sending NACK.\n");
        uart_putc(UART0_ID, NACK);
    }

    int result = snprintf(error_message, sizeof(error_message), "%c(%s)%c", STX, error_text, ETX);
    bccGenerate((uint8_t *)error_message, result, &error_message_xor);
    
    PRINTF("SENDERRORMESSAGE: error message xor is: %02X\n", error_message_xor);
    PRINTF("SENDERRORMESSAGE: error message generated is:\n");
    printBufferHex((uint8_t *)error_message, result);
    PRINTF("\n");

    uart_puts(UART0_ID, error_message);
    uart_putc(UART0_ID, error_message_xor);
}

#endif
