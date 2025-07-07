#ifndef SD_CARD_H
#define SD_CARD_H

#include <stdint.h>
#include <stdbool.h>
#include "hardware/spi.h"

// SD Card Commands
#define SD_CMD0     0   // GO_IDLE_STATE
#define SD_CMD1     1   // SEND_OP_COND
#define SD_CMD8     8   // SEND_IF_COND
#define SD_CMD9     9   // SEND_CSD
#define SD_CMD10    10  // SEND_CID
#define SD_CMD12    12  // STOP_TRANSMISSION
#define SD_CMD13    13  // SEND_STATUS
#define SD_CMD16    16  // SET_BLOCKLEN
#define SD_CMD17    17  // READ_SINGLE_BLOCK
#define SD_CMD18    18  // READ_MULTIPLE_BLOCK
#define SD_CMD23    23  // SET_BLOCK_COUNT
#define SD_CMD24    24  // WRITE_BLOCK
#define SD_CMD25    25  // WRITE_MULTIPLE_BLOCK
#define SD_CMD41    41  // SEND_OP_COND (ACMD)
#define SD_CMD55    55  // APP_CMD
#define SD_CMD58    58  // READ_OCR

// SD Card Response Types
#define SD_R1_IDLE_STATE         0x01
#define SD_R1_ERASE_RESET        0x02
#define SD_R1_ILLEGAL_COMMAND    0x04
#define SD_R1_COM_CRC_ERROR      0x08
#define SD_R1_ERASE_SEQUENCE_ERROR 0x10
#define SD_R1_ADDRESS_ERROR      0x20
#define SD_R1_PARAMETER_ERROR    0x40

// GPIO pins for SD card SPI (defined in main.c)
extern const uint SD_PIN_MISO;
extern const uint SD_PIN_CS;
extern const uint SD_PIN_SCK;
extern const uint SD_PIN_MOSI;
extern spi_inst_t* const SD_SPI_PORT;

// Function prototypes
int sd_init_driver(void);
int sd_read_sectors(void* buffer, uint32_t sector, uint32_t count);
int sd_write_sectors(const void* buffer, uint32_t sector, uint32_t count);
uint32_t sd_get_sectors_count(void);

#endif // SD_CARD_H
