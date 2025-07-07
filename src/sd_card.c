#include "sd_card.h"
#include "hardware/spi.h"
#include "hardware/gpio.h"
#include "pico/stdlib.h"
#include <string.h>
#include <stdio.h>

static uint32_t sd_sectors = 0;
static bool sd_initialized = false;

// Helper functions
static void sd_cs_select(void) {
    gpio_put(SD_PIN_CS, 0);
}

static void sd_cs_deselect(void) {
    gpio_put(SD_PIN_CS, 1);
}

static uint8_t sd_spi_write(uint8_t data) {
    uint8_t rx_data;
    spi_write_read_blocking(SD_SPI_PORT, &data, &rx_data, 1);
    return rx_data;
}

static uint8_t sd_send_command(uint8_t cmd, uint32_t arg) {
    uint8_t crc = 0;
    
    // Calculate CRC for CMD0 and CMD8
    if (cmd == SD_CMD0) crc = 0x95;
    else if (cmd == SD_CMD8) crc = 0x87;
    
    // Send command
    sd_spi_write(0x40 | cmd);
    sd_spi_write((arg >> 24) & 0xFF);
    sd_spi_write((arg >> 16) & 0xFF);
    sd_spi_write((arg >> 8) & 0xFF);
    sd_spi_write(arg & 0xFF);
    sd_spi_write(crc);
    
    // Wait for response
    uint8_t response;
    for (int i = 0; i < 10; i++) {
        response = sd_spi_write(0xFF);
        if (response != 0xFF) break;
    }
    
    return response;
}

int sd_init_driver(void) {
    sd_cs_deselect();
    
    // Send 80+ clock cycles with CS high
    for (int i = 0; i < 10; i++) {
        sd_spi_write(0xFF);
    }
    
    // CMD0: GO_IDLE_STATE
    sd_cs_select();
    uint8_t response = sd_send_command(SD_CMD0, 0);
    sd_cs_deselect();
    
    if (response != SD_R1_IDLE_STATE) {
        printf("CMD0 failed: 0x%02X\n", response);
        return -1;
    }
    
    // CMD8: SEND_IF_COND
    sd_cs_select();
    response = sd_send_command(SD_CMD8, 0x1AA);
    if (response == SD_R1_IDLE_STATE) {
        // Read additional response bytes
        uint8_t r7[4];
        for (int i = 0; i < 4; i++) {
            r7[i] = sd_spi_write(0xFF);
        }
        printf("CMD8 response: %02X %02X %02X %02X\n", r7[0], r7[1], r7[2], r7[3]);
    }
    sd_cs_deselect();
    
    // Initialize card with ACMD41
    int timeout = 1000;
    do {
        sd_cs_select();
        sd_send_command(SD_CMD55, 0);
        response = sd_send_command(SD_CMD41, 0x40000000);
        sd_cs_deselect();
        
        if (response == 0) break;
        
        sleep_ms(1);
        timeout--;
    } while (timeout > 0);
    
    if (response != 0) {
        printf("ACMD41 failed: 0x%02X\n", response);
        return -1;
    }
    
    // Set block size to 512 bytes
    sd_cs_select();
    response = sd_send_command(SD_CMD16, 512);
    sd_cs_deselect();
    
    if (response != 0) {
        printf("CMD16 failed: 0x%02X\n", response);
        return -1;
    }
    
    // Get card capacity (simplified - assume 1GB for now)
    // In a real implementation, you'd read the CSD register
    sd_sectors = 2097152; // 1GB = 2097152 * 512 bytes
    
    sd_initialized = true;
    printf("SD card initialized successfully\n");
    return 0;
}

int sd_read_sectors(void* buffer, uint32_t sector, uint32_t count) {
    if (!sd_initialized) {
        printf("SD card not initialized\n");
        return -1;
    }
    
    uint8_t* buf = (uint8_t*)buffer;
    
    for (uint32_t i = 0; i < count; i++) {
        sd_cs_select();
        
        uint8_t response = sd_send_command(SD_CMD17, sector + i);
        if (response != 0) {
            printf("CMD17 failed: 0x%02X\n", response);
            sd_cs_deselect();
            return -1;
        }
        
        // Wait for data token
        uint8_t token;
        int timeout = 1000;
        do {
            token = sd_spi_write(0xFF);
            timeout--;
        } while (token != 0xFE && timeout > 0);
        
        if (token != 0xFE) {
            printf("Data token timeout: 0x%02X\n", token);
            sd_cs_deselect();
            return -1;
        }
        
        // Read data
        for (int j = 0; j < 512; j++) {
            buf[i * 512 + j] = sd_spi_write(0xFF);
        }
        
        // Read CRC (ignore)
        sd_spi_write(0xFF);
        sd_spi_write(0xFF);
        
        sd_cs_deselect();
    }
    
    return 0;
}

int sd_write_sectors(const void* buffer, uint32_t sector, uint32_t count) {
    if (!sd_initialized) {
        printf("SD card not initialized\n");
        return -1;
    }
    
    const uint8_t* buf = (const uint8_t*)buffer;
    
    for (uint32_t i = 0; i < count; i++) {
        sd_cs_select();
        
        uint8_t response = sd_send_command(SD_CMD24, sector + i);
        if (response != 0) {
            printf("CMD24 failed: 0x%02X\n", response);
            sd_cs_deselect();
            return -1;
        }
        
        // Send data token
        sd_spi_write(0xFE);
        
        // Send data
        for (int j = 0; j < 512; j++) {
            sd_spi_write(buf[i * 512 + j]);
        }
        
        // Send dummy CRC
        sd_spi_write(0xFF);
        sd_spi_write(0xFF);
        
        // Wait for response
        uint8_t data_response = sd_spi_write(0xFF);
        if ((data_response & 0x1F) != 0x05) {
            printf("Write response error: 0x%02X\n", data_response);
            sd_cs_deselect();
            return -1;
        }
        
        // Wait for write completion
        int timeout = 1000;
        do {
            response = sd_spi_write(0xFF);
            timeout--;
        } while (response == 0 && timeout > 0);
        
        if (timeout == 0) {
            printf("Write timeout\n");
            sd_cs_deselect();
            return -1;
        }
        
        sd_cs_deselect();
    }
    
    return 0;
}

uint32_t sd_get_sectors_count(void) {
    return sd_sectors;
}
