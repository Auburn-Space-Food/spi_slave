#include <Arduino.h>

#include "spi_slave.h"

uint16_t data_out = 0xFFFF;
uint16_t data_out2[2] = {0x1111, 0xFFFF};
uint16_t data_in;

void setup() {
  spi_slave_enable();
}

void loop() {
  if (spi_slave_transfer_complete) {
    spi_slave_send_message(&data_out);
    // or:
    // spi_slave_send_message(data_out2, 2);
  }

  while (spi_slave_message_pending) {
    spi_slave_get_message(&data_in);
  }
}
