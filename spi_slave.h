#ifndef SPI_SLAVE_H
#define SPI_SLAVE_H

#ifdef __cplusplus
extern "C" {
#endif
#include <stdint.h>

// Set the inactive state of SCLK, see section 32.8.9 of the datasheet
#define SPI_POLARITY 0 // SCLK inactive low
// #define SPI_POLARITY 1 // SCLK inactive high

// Set the clock phase for data change and sampling, see section 32.8.9
#define SPI_PHASE 0 // Change data on the trailing edge of the preceding clock
//#define SPI_PHASE 1 // Change data on the leading edge of the current clock 

#define SPI_WORD_LENGTH 16 // See section 32.8.9, word length must be 8 or 16
                           // without modification of spi_slave.

#if SPI_WORD_LENGTH == 16
#define SPI_DATA_T uint16_t
#elif SPI_WORD_LENGTH == 8
#define SPI_DATA_T uint8_t
#endif

#if SPI_WORD_LENGTH != 8 && SPI_WORD_LENGTH != 16
#error "Invalid SPI_WORD_LENGTH"
#endif

#define SPI_RX_BUFLEN 8 // Hold 8 unread messages before overflow

extern volatile int spi_slave_transfer_complete;
extern volatile int spi_slave_message_pending;
extern volatile int spi_slave_message_overflow;

typedef struct message_format {
    uint8_t length;
    uint8_t type;
    uint8_t *data;
} message_format_t;

void spi_slave_enable();
void spi_slave_disable();

int spi_slave_send_message(SPI_DATA_T *data);
int spi_slave_send_messages(SPI_DATA_T *data, size_t len);
int spi_slave_get_message(SPI_DATA_T *message);

#ifdef __cplusplus
}
#endif

#endif //SPI_SLAVE_H
