// ===================================================================================
// SPI1 Master Functions (TX only) for CH32V203                               * v1.1 *
// ===================================================================================

#include "spi.h"

// Init SPI1 (PA5: SCK, PA7: MOSI, transmit-only master)
void SPI1_init(void)
{
    // Enable GPIO and SPI module clock
    RCC->APB2PCENR |= RCC_AFIOEN | RCC_IOPAEN | RCC_SPI1EN;

    // Setup GPIO pins PA5 (SCK) and PA7 (MOSI): alternate function push-pull, 50MHz
    GPIOA->CFGLR = (GPIOA->CFGLR & ~(((uint32_t)0b1111 << (5 << 2)) | ((uint32_t)0b1111 << (7 << 2))))
                   | (((uint32_t)0b1011 << (5 << 2)) | ((uint32_t)0b1011 << (7 << 2)));

    // Setup and enable SPI master, standard configuration
    SPI1->CTLR1 = (SPI1_PRESC << 3)    // set prescaler
                  | SPI_CTLR1_MSTR     // master configuration
                  | SPI_CTLR1_BIDIMODE // one-line mode
                  | SPI_CTLR1_BIDIOE   // transmit only
                  | SPI_CTLR1_SSM      // software control of NSS
                  | SPI_CTLR1_SSI      // set internal NSS high
                  | SPI_CTLR1_SPE;     // enable SPI
}

// Transmit one data byte
void SPI1_write(uint8_t data)
{
    while (!SPI1_ready())
        ;               // wait for ready to write
    SPI1->DATAR = data; // send data byte
}

// Transmit data bytes from buffer
void SPI1_writeBuffer(const uint8_t* buf, uint16_t len)
{
    while (len--) SPI1_write(*buf++);
}

// Wait until the last byte has been shifted out
void SPI1_flush(void)
{
    while (!SPI1_ready())
        ; // wait for transmit buffer empty
    while (SPI1_busy())
        ; // wait for shift register empty
}
