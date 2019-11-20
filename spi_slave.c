#ifdef __cplusplus
extern "C" {
#endif

// See the SAM3X datasheet for more information:
// http://ww1.microchip.com/downloads/en/devicedoc/atmel-11057-32-bit-cortex-m3-microcontroller-sam3x-sam3a_datasheet.pdf

// For register definitions in C, see the following links:
// https://github.com/arduino-org/Arduino/blob/master/hardware/arduino/sam/system/CMSIS/Device/ATMEL/sam3xa/include/component/component_spi.h
// https://github.com/arduino-org/Arduino/blob/master/hardware/arduino/sam/system/CMSIS/Device/ATMEL/sam3xa/include/sam3x8e.h
// It was incredibly difficult to find these definitions

#include <sam.h>

#include <stdint.h>
#include <stddef.h>

#include "spi_slave.h"

volatile int spi_slave_transfer_complete = 1;
volatile int spi_slave_message_pending = 0;
volatile int spi_slave_message_overflow = 0;

static SPI_DATA_T *tx_buffer = NULL;
static size_t tx_length = 0;
volatile static size_t tx_pos = 0;

typedef struct circular_buffer {
    SPI_DATA_T *buffer;
    SPI_DATA_T *buffer_end;

    SPI_DATA_T *head;
    SPI_DATA_T *tail;

    size_t capacity;
    size_t count;
} circular_buffer_t;

static SPI_DATA_T rx_buffer[SPI_RX_BUFLEN];
static circular_buffer_t rx_cb;

void spi_slave_enable() {
    // Reset SPI Interface
    SPI0->SPI_CR |= SPI_CR_SWRST;

    // Setup pins for SPI operation, see sections 32.6.1 and 31.5 of the datasheet
    PIOA->PIO_PER &= ~PIO_PER_P25; // Disable digital IO mode on PA25 (SPI0_MISO)
    PIOA->PIO_PER &= ~PIO_PER_P26; // Disable digital IO mode on PA26 (SPI0_MOSI)
    PIOA->PIO_PER &= ~PIO_PER_P27; // Disable digital IO mode on PA27 (SPI0_SPCK)
    PIOA->PIO_PER &= ~PIO_PER_P28; // Disable digital IO mode on PA28 (SPI0_NPCS0) 

    PIOA->PIO_ABSR &= ~PIO_PER_P25; // Enable peripheral A mode on PA25
    PIOA->PIO_ABSR &= ~PIO_PER_P26; // Enable peripheral A mode on PA26
    PIOA->PIO_ABSR &= ~PIO_PER_P27; // Enable peripheral A mode on PA27
    PIOA->PIO_ABSR &= ~PIO_PER_P28; // Enable peripheral A mode on PA28

    // Set SPI to slave mode
    SPI0->SPI_MR &= ~SPI_MR_MSTR;

    // Set clock polarity and phase
    SPI0->SPI_CSR[0] |= (SPI_CSR_CPOL & (SPI_POLARITY << SPI_CSR_CPOL)) | 
                        (SPI_CSR_NCPHA & (SPI_PHASE << SPI_CSR_NCPHA));

    // Set the word length
    SPI0->SPI_CSR[0] |= ((SPI_WORD_LENGTH - 8) << SPI_CSR_BITS_Pos) & SPI_CSR_BITS_Msk;

    // Enable the SPI
    SPI0->SPI_CR |= SPI_CR_SPIEN;

    // Enable the slave interrupts
    SPI0->SPI_IER |= SPI_IER_RDRF;

    // Enable NVIC interrupt
    // TODO: determine if this is necessary and sufficient
    //NVIC_ClearPending(SPI0_IRQn);
    //NVIC_EnableIRQ(SPI0_IRQn);

    // Set defaults on state variables
    spi_slave_transfer_complete = 1;
    spi_slave_message_pending = 0;
    spi_slave_message_overflow = 0;

    rx_cb.buffer = rx_buffer;
    rx_cb.buffer_end = rx_buffer + SPI_RX_BUFLEN;

    rx_cb.head = rx_buffer;
    rx_cb.tail = rx_buffer;

    rx_cb.capacity = SPI_RX_BUFLEN;
    rx_cb.count = 0;

    tx_buffer = NULL;
}

void spi_slave_disable() {
    // Disable SPI, should finish last transfer before disabling
    SPI0->SPI_CR |= SPI_CR_SPIDIS;
}

int spi_slave_send_message(SPI_DATA_T *data) {
    return spi_slave_send_messages(data, 1);
}

int spi_slave_send_messages(SPI_DATA_T *data, size_t len) {
    if (spi_slave_transfer_complete) {
        return 1;
    }

    tx_buffer = (SPI_DATA_T *) data;
    tx_length = len;
    tx_pos = 0;
    
    spi_slave_transfer_complete = 0;

    SPI0->SPI_IER |= SPI_IER_TDRE; // Enable the transmit data register empty interrupt

    return 0;
}

int spi_slave_get_message(SPI_DATA_T *message) {
    if (!spi_slave_message_pending) {
        return 1;
    }

    SPI0->SPI_IDR |= SPI_IDR_RDRF; // Disable RDRF interrupts, entering a critical section

    *message = *(rx_cb.tail);

    rx_cb.tail++;
    if (rx_cb.tail == rx_cb.buffer_end) {
        rx_cb.tail = rx_cb.buffer;
    }

    rx_cb.count--;
    if (rx_cb.count == 0) {
        spi_slave_message_pending = 0;
    }

    spi_slave_message_overflow = 0;

    SPI0->SPI_IER |= SPI_IER_RDRF; // Re-enable RDRF
}

void SPI0_IRQHandler() {
    if (SPI0->SPI_SR & SPI_SR_RDRF) {
        if (rx_cb.count == rx_cb.capacity) {
            spi_slave_message_overflow = 1;
        } else {
#if SPI_WORD_LENGTH == 8
            *(rx_cb.head) = (SPI_DATA_T) (SPI0->SPI_RDR & 0x00FF);
#elif SPI_WORD_LENGTH == 16
            *(rx_cb.head) = (SPI_DATA_T) (SPI0->SPI_RDR & 0xFFFF);
#endif
            
            rx_cb.head++;
            rx_cb.count++;
            if (rx_cb.head == rx_cb.buffer_end) {
                rx_cb.head = rx_cb.buffer;
            }

            spi_slave_message_pending = 1;
        }
    }

    if (SPI0->SPI_SR & SPI_SR_TDRE) {
        if (tx_length == tx_pos) {
            // If we've reached the end of the transmit array,
            // null out the array and disable the transmit interrupt
            tx_buffer = NULL;
            SPI0->SPI_IDR |= SPI_IDR_TDRE;

            spi_slave_transfer_complete = 1;
        } else {
#if SPI_WORD_LENGTH == 8
            SPI0->SPI_TDR |= tx_buffer[tx_pos];
#elif SPI_WORD_LENGTH == 16
            // Transmit the 16-bit message in network (big) endian format
            SPI0->SPI_TDR |= (tx_buffer[tx_pos] & 0xFF00) << 8;
            SPI0->SPI_TDR |= tx_buffer[tx_pos] & 0x00FF;
#endif
            tx_pos++;
        }
    }

    //NVIC_ClearPending(SPI0_IRQn);
}
#ifdef __cplusplus
}
#endif
