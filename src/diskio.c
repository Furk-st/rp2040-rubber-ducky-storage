#include "ff.h"
#include "diskio.h"
#include "sd_card.h"
#include <stdio.h>

DSTATUS disk_initialize(BYTE pdrv) {
    printf("disk_initialize(%d)\n", pdrv);
    if (pdrv != 0) return STA_NOINIT;
    
    if (sd_init_driver() == 0) {
        return 0;
    }
    return STA_NOINIT;
}

DSTATUS disk_status(BYTE pdrv) {
    if (pdrv != 0) return STA_NOINIT;
    return 0;
}

DRESULT disk_read(BYTE pdrv, BYTE* buff, LBA_t sector, UINT count) {
    printf("disk_read(pdrv=%d, sector=%lu, count=%u)\n", pdrv, sector, count);
    if (pdrv != 0) return RES_PARERR;
    
    if (sd_read_sectors(buff, sector, count) == 0) {
        return RES_OK;
    }
    return RES_ERROR;
}

DRESULT disk_write(BYTE pdrv, const BYTE* buff, LBA_t sector, UINT count) {
    printf("disk_write(pdrv=%d, sector=%lu, count=%u)\n", pdrv, sector, count);
    if (pdrv != 0) return RES_PARERR;
    
    if (sd_write_sectors(buff, sector, count) == 0) {
        return RES_OK;
    }
    return RES_ERROR;
}

DRESULT disk_ioctl(BYTE pdrv, BYTE cmd, void* buff) {
    printf("disk_ioctl(pdrv=%d, cmd=%d)\n", pdrv, cmd);
    if (pdrv != 0) return RES_PARERR;
    
    switch (cmd) {
        case CTRL_SYNC:
            return RES_OK;
        
        case GET_SECTOR_COUNT:
            *(DWORD*)buff = sd_get_sectors_count();
            printf("GET_SECTOR_COUNT: %lu\n", *(DWORD*)buff);
            return RES_OK;
        
        case GET_SECTOR_SIZE:
            *(WORD*)buff = 512;
            return RES_OK;
        
        case GET_BLOCK_SIZE:
            *(DWORD*)buff = 1;
            return RES_OK;
        
        default:
            return RES_PARERR;
    }
}

DWORD get_fattime(void) {
    // Return a fixed timestamp (Jan 1, 2023, 00:00:00)
    // Format: bit31:25=Year(0-127 org.1980), bit24:21=Month(1-12), bit20:16=Day(1-31)
    //         bit15:11=Hour(0-23), bit10:5=Minute(0-59), bit4:0=Second/2(0-29)
    return ((DWORD)(2023 - 1980) << 25) |   // Year
           ((DWORD)1 << 21) |               // Month
           ((DWORD)1 << 16) |               // Day
           ((DWORD)0 << 11) |               // Hour
           ((DWORD)0 << 5) |                // Minute
           ((DWORD)0 << 0);                 // Second/2
}
