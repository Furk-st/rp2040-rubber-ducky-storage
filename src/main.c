#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <ctype.h>

#include "bsp/board.h"
#include "tusb.h"
#include "pico/stdlib.h"
#include "hardware/gpio.h"
#include "hardware/spi.h"
#include "ff.h"
#include "diskio.h"
#include "sd_card.h"

// GPIO pins for SD card SPI
spi_inst_t* const SD_SPI_PORT = spi0;
const uint SD_PIN_MISO = 4;
const uint SD_PIN_CS   = 5;
const uint SD_PIN_SCK  = 2;
const uint SD_PIN_MOSI = 3;

// LED pin
#define LED_PIN 25

// USB HID Report IDs
enum {
    REPORT_ID_KEYBOARD = 1,
    REPORT_ID_MOUSE,
    REPORT_ID_CONSUMER_CONTROL,
    REPORT_ID_GAMEPAD,
    REPORT_ID_COUNT
};

// Ducky script variables
static char ducky_script[8192];
static bool script_loaded = false;
static bool script_running = false;
static size_t script_pos = 0;
static uint32_t last_key_time = 0;
static uint32_t key_delay = 50; // Default delay in ms

// SD card variables
static FATFS fs;
static bool sd_mounted = false;

// Function prototypes
void load_ducky_script(void);
void process_ducky_script(void);
void send_hid_report(uint8_t modifier, uint8_t keycode);
void parse_ducky_command(char* line);
uint8_t char_to_keycode(char c);
void init_sd_card(void);
void blink_led(int count);

//--------------------------------------------------------------------+
// HID Report Descriptor (declare this first)
//--------------------------------------------------------------------+

uint8_t const desc_hid_report[] = {
    TUD_HID_REPORT_DESC_KEYBOARD(HID_REPORT_ID(REPORT_ID_KEYBOARD)),
};

//--------------------------------------------------------------------+
// Device Descriptors
//--------------------------------------------------------------------+

uint8_t const* tud_descriptor_device_cb(void) {
    static tusb_desc_device_t const desc_device = {
        .bLength            = sizeof(tusb_desc_device_t),
        .bDescriptorType    = TUSB_DESC_DEVICE,
        .bcdUSB             = 0x0200,
        .bDeviceClass       = 0x00,
        .bDeviceSubClass    = 0x00,
        .bDeviceProtocol    = 0x00,
        .bMaxPacketSize0    = CFG_TUD_ENDPOINT0_SIZE,
        .idVendor           = 0xCafe,
        .idProduct          = 0x4001,
        .bcdDevice          = 0x0100,
        .iManufacturer      = 0x01,
        .iProduct           = 0x02,
        .iSerialNumber      = 0x03,
        .bNumConfigurations = 0x01
    };
    return (uint8_t const*) &desc_device;
}

// Configuration Descriptor
enum {
    ITF_NUM_HID,
    ITF_NUM_MSC,
    ITF_NUM_TOTAL
};

#define CONFIG_TOTAL_LEN (TUD_CONFIG_DESC_LEN + TUD_HID_DESC_LEN + TUD_MSC_DESC_LEN)
#define EPNUM_HID   0x81
#define EPNUM_MSC_OUT 0x02
#define EPNUM_MSC_IN  0x82

uint8_t const desc_configuration[] = {
    TUD_CONFIG_DESCRIPTOR(1, ITF_NUM_TOTAL, 0, CONFIG_TOTAL_LEN, TUSB_DESC_CONFIG_ATT_REMOTE_WAKEUP, 100),
    TUD_HID_DESCRIPTOR(ITF_NUM_HID, 0, HID_ITF_PROTOCOL_KEYBOARD, sizeof(desc_hid_report), EPNUM_HID, CFG_TUD_HID_EP_BUFSIZE, 1),
    TUD_MSC_DESCRIPTOR(ITF_NUM_MSC, 0, EPNUM_MSC_OUT, EPNUM_MSC_IN, 64),
};

uint8_t const* tud_descriptor_configuration_cb(uint8_t index) {
    (void) index;
    return desc_configuration;
}

uint8_t const* tud_hid_descriptor_report_cb(uint8_t instance) {
    (void) instance;
    return desc_hid_report;
}

//--------------------------------------------------------------------+
// String Descriptors
//--------------------------------------------------------------------+

char const* string_desc_arr[] = {
    (const char[]) { 0x09, 0x04 },
    "RubberDucky",
    "Pico Ducky Storage",
    "123456",
};

static uint16_t _desc_str[32];

uint16_t const* tud_descriptor_string_cb(uint8_t index, uint16_t langid) {
    (void) langid;
    uint8_t chr_count;

    if (index == 0) {
        memcpy(&_desc_str[1], string_desc_arr[0], 2);
        chr_count = 1;
    } else {
        if (!(index < sizeof(string_desc_arr)/sizeof(string_desc_arr[0]))) return NULL;
        const char* str = string_desc_arr[index];
        chr_count = strlen(str);
        if (chr_count > 31) chr_count = 31;
        for(uint8_t i=0; i<chr_count; i++) {
            _desc_str[1+i] = str[i];
        }
    }
    _desc_str[0] = (TUSB_DESC_STRING << 8 ) | (2*chr_count + 2);
    return _desc_str;
}

//--------------------------------------------------------------------+
// SD Card and File System Functions
//--------------------------------------------------------------------+

void init_sd_card(void) {
    gpio_init(SD_PIN_MISO);
    gpio_init(SD_PIN_CS);
    gpio_init(SD_PIN_SCK);
    gpio_init(SD_PIN_MOSI);
    
    gpio_set_function(SD_PIN_MISO, GPIO_FUNC_SPI);
    gpio_set_function(SD_PIN_SCK, GPIO_FUNC_SPI);
    gpio_set_function(SD_PIN_MOSI, GPIO_FUNC_SPI);
    gpio_set_function(SD_PIN_CS, GPIO_FUNC_SIO);
    
    gpio_set_dir(SD_PIN_CS, GPIO_OUT);
    gpio_put(SD_PIN_CS, 1);
    
    spi_init(SD_SPI_PORT, 400000); // Start with 400kHz
    
    if (sd_init_driver() == 0) {
        FRESULT fr = f_mount(&fs, "", 1);
        if (fr == FR_OK) {
            sd_mounted = true;
            printf("SD card mounted successfully\n");
            blink_led(3);
        } else {
            printf("Failed to mount SD card: %d\n", fr);
            blink_led(5);
        }
    } else {
        printf("SD card initialization failed\n");
        blink_led(5);
    }
}

void load_ducky_script(void) {
    if (!sd_mounted) {
        printf("SD card not mounted, using default script...\n");
        strcpy(ducky_script, "DELAY 1000\nGUI r\nDELAY 500\nSTRING notepad\nENTER\nDELAY 1000\nSTRING Hello from Pico Ducky!\n");
        script_loaded = true;
        return;
    }
    
    FIL file;
    FRESULT fr = f_open(&file, "ducky.txt", FA_READ);
    
    if (fr != FR_OK) {
        printf("No ducky.txt file found, using default script\n");
        strcpy(ducky_script, "DELAY 1000\nGUI r\nDELAY 500\nSTRING notepad\nENTER\nDELAY 1000\nSTRING Hello from Pico Ducky!\n");
        script_loaded = true;
        return;
    }
    
    UINT bytes_read;
    fr = f_read(&file, ducky_script, sizeof(ducky_script) - 1, &bytes_read);
    f_close(&file);
    
    if (fr == FR_OK) {
        ducky_script[bytes_read] = '\0';
        script_loaded = true;
        printf("Ducky script loaded: %d bytes\n", bytes_read);
    } else {
        printf("Failed to read ducky.txt\n");
    }
}

//--------------------------------------------------------------------+
// Ducky Script Processing
//--------------------------------------------------------------------+

void process_ducky_script(void) {
    if (!script_loaded || !script_running) return;
    
    uint32_t current_time = board_millis();
    if (current_time - last_key_time < key_delay) return;
    
    // Find next command
    char line[256];
    size_t line_pos = 0;
    
    // Skip to next line if we're at newline
    while (script_pos < strlen(ducky_script) && 
           (ducky_script[script_pos] == '\n' || ducky_script[script_pos] == '\r')) {
        script_pos++;
    }
    
    // Check if we've reached the end
    if (script_pos >= strlen(ducky_script)) {
        script_running = false;
        printf("Script execution completed\n");
        return;
    }
    
    // Read line
    while (script_pos < strlen(ducky_script) && 
           ducky_script[script_pos] != '\n' && 
           ducky_script[script_pos] != '\r' && 
           line_pos < sizeof(line) - 1) {
        line[line_pos++] = ducky_script[script_pos++];
    }
    line[line_pos] = '\0';
    
    if (strlen(line) > 0) {
        parse_ducky_command(line);
    }
    
    last_key_time = current_time;
}

void parse_ducky_command(char* line) {
    // Remove leading/trailing whitespace
    while (*line == ' ' || *line == '\t') line++;
    
    if (strncmp(line, "DELAY ", 6) == 0) {
        key_delay = atoi(line + 6);
        printf("Set delay to %d ms\n", key_delay);
    }
    else if (strncmp(line, "STRING ", 7) == 0) {
        char* text = line + 7;
        for (int i = 0; text[i]; i++) {
            uint8_t keycode = char_to_keycode(text[i]);
            uint8_t modifier = 0;
            
            if (isupper(text[i]) || strchr("!@#$%^&*()_+{}|:\"<>?", text[i])) {
                modifier = KEYBOARD_MODIFIER_LEFTSHIFT;
            }
            
            send_hid_report(modifier, keycode);
            sleep_ms(50);
            send_hid_report(0, 0); // Release key
            sleep_ms(50);
        }
    }
    else if (strcmp(line, "ENTER") == 0) {
        send_hid_report(0, HID_KEY_ENTER);
        sleep_ms(50);
        send_hid_report(0, 0);
    }
    else if (strcmp(line, "SPACE") == 0) {
        send_hid_report(0, HID_KEY_SPACE);
        sleep_ms(50);
        send_hid_report(0, 0);
    }
    else if (strncmp(line, "GUI ", 4) == 0) {
        char key = line[4];
        uint8_t keycode = char_to_keycode(key);
        send_hid_report(KEYBOARD_MODIFIER_LEFTGUI, keycode);
        sleep_ms(50);
        send_hid_report(0, 0);
    }
    else if (strcmp(line, "TAB") == 0) {
        send_hid_report(0, HID_KEY_TAB);
        sleep_ms(50);
        send_hid_report(0, 0);
    }
    else if (strcmp(line, "ESCAPE") == 0) {
        send_hid_report(0, HID_KEY_ESCAPE);
        sleep_ms(50);
        send_hid_report(0, 0);
    }
}

uint8_t char_to_keycode(char c) {
    if (c >= 'a' && c <= 'z') return HID_KEY_A + (c - 'a');
    if (c >= 'A' && c <= 'Z') return HID_KEY_A + (c - 'A');
    if (c >= '1' && c <= '9') return HID_KEY_1 + (c - '1');
    if (c == '0') return HID_KEY_0;
    if (c == ' ') return HID_KEY_SPACE;
    if (c == '\t') return HID_KEY_TAB;
    if (c == '\n') return HID_KEY_ENTER;
    
    // Special characters
    switch(c) {
        case '!': return HID_KEY_1;
        case '@': return HID_KEY_2;
        case '#': return HID_KEY_3;
        case '$': return HID_KEY_4;
        case '%': return HID_KEY_5;
        case '^': return HID_KEY_6;
        case '&': return HID_KEY_7;
        case '*': return HID_KEY_8;
        case '(': return HID_KEY_9;
        case ')': return HID_KEY_0;
        case '-': return HID_KEY_MINUS;
        case '_': return HID_KEY_MINUS;
        case '=': return HID_KEY_EQUAL;
        case '+': return HID_KEY_EQUAL;
        case '[': return HID_KEY_BRACKET_LEFT;
        case '{': return HID_KEY_BRACKET_LEFT;
        case ']': return HID_KEY_BRACKET_RIGHT;
        case '}': return HID_KEY_BRACKET_RIGHT;
        case '\\': return HID_KEY_BACKSLASH;
        case '|': return HID_KEY_BACKSLASH;
        case ';': return HID_KEY_SEMICOLON;
        case ':': return HID_KEY_SEMICOLON;
        case '\'': return HID_KEY_APOSTROPHE;
        case '"': return HID_KEY_APOSTROPHE;
        case '`': return HID_KEY_GRAVE;
        case '~': return HID_KEY_GRAVE;
        case ',': return HID_KEY_COMMA;
        case '<': return HID_KEY_COMMA;
        case '.': return HID_KEY_PERIOD;
        case '>': return HID_KEY_PERIOD;
        case '/': return HID_KEY_SLASH;
        case '?': return HID_KEY_SLASH;
        default: return 0;
    }
}

void send_hid_report(uint8_t modifier, uint8_t keycode) {
    if (tud_hid_ready()) {
        uint8_t keycode_array[6] = {0};
        if (keycode != 0) {
            keycode_array[0] = keycode;
        }
        tud_hid_keyboard_report(REPORT_ID_KEYBOARD, modifier, keycode_array);
    }
}

//--------------------------------------------------------------------+
// USB HID Callbacks
//--------------------------------------------------------------------+

uint16_t tud_hid_get_report_cb(uint8_t instance, uint8_t report_id, hid_report_type_t report_type, uint8_t* buffer, uint16_t reqlen) {
    (void) instance;
    (void) report_id;
    (void) report_type;
    (void) buffer;
    (void) reqlen;
    return 0;
}

void tud_hid_set_report_cb(uint8_t instance, uint8_t report_id, hid_report_type_t report_type, uint8_t const* buffer, uint16_t bufsize) {
    (void) instance;
    (void) report_id;
    (void) report_type;
    (void) buffer;
    (void) bufsize;
}

//--------------------------------------------------------------------+
// MSC (Mass Storage Class) Callbacks
//--------------------------------------------------------------------+

void tud_msc_inquiry_cb(uint8_t lun, uint8_t vendor_id[8], uint8_t product_id[16], uint8_t product_rev[4]) {
    (void) lun;
    const char vid[] = "PicoDuck";
    const char pid[] = "Mass Storage";
    const char rev[] = "1.0";
    
    memcpy(vendor_id, vid, strlen(vid));
    memcpy(product_id, pid, strlen(pid));
    memcpy(product_rev, rev, strlen(rev));
}

bool tud_msc_test_unit_ready_cb(uint8_t lun) {
    (void) lun;
    return sd_mounted;
}

void tud_msc_capacity_cb(uint8_t lun, uint32_t* block_count, uint16_t* block_size) {
    (void) lun;
    *block_size = 512;
    if (sd_mounted) {
        *block_count = sd_get_sectors_count();
    } else {
        *block_count = 0;
    }
}

bool tud_msc_start_stop_cb(uint8_t lun, uint8_t power_condition, bool start, bool load_eject) {
    (void) lun;
    (void) power_condition;
    (void) start;
    (void) load_eject;
    return true;
}

int32_t tud_msc_read10_cb(uint8_t lun, uint32_t lba, uint32_t offset, void* buffer, uint32_t bufsize) {
    (void) lun;
    
    if (!sd_mounted) return -1;
    
    if (sd_read_sectors(buffer, lba + offset/512, bufsize/512) == 0) {
        return bufsize;
    }
    return -1;
}

int32_t tud_msc_write10_cb(uint8_t lun, uint32_t lba, uint32_t offset, uint8_t* buffer, uint32_t bufsize) {
    (void) lun;
    
    if (!sd_mounted) return -1;
    
    if (sd_write_sectors(buffer, lba + offset/512, bufsize/512) == 0) {
        return bufsize;
    }
    return -1;
}

void tud_msc_write10_complete_cb(uint8_t lun) {
    (void) lun;
    if (sd_mounted) {
        f_sync(NULL);
    }
}

int32_t tud_msc_scsi_cb(uint8_t lun, uint8_t const scsi_cmd[16], void* buffer, uint16_t bufsize) {
    void const* response = NULL;
    int32_t resplen = 0;
    
    switch (scsi_cmd[0]) {
        default:
            tud_msc_set_sense(lun, SCSI_SENSE_ILLEGAL_REQUEST, 0x20, 0x00);
            resplen = -1;
            break;
    }
    
    if (resplen > bufsize) resplen = bufsize;
    
    if (response && (resplen > 0)) {
        memcpy(buffer, response, resplen);
    }
    
    return resplen;
}

//--------------------------------------------------------------------+
// Utility Functions
//--------------------------------------------------------------------+

void blink_led(int count) {
    for (int i = 0; i < count; i++) {
        gpio_put(LED_PIN, 1);
        sleep_ms(100);
        gpio_put(LED_PIN, 0);
        sleep_ms(100);
    }
}

//--------------------------------------------------------------------+
// Main Function
//--------------------------------------------------------------------+

int main(void) {
    board_init();
    
    gpio_init(LED_PIN);
    gpio_set_dir(LED_PIN, GPIO_OUT);
    
    printf("Pico Ducky with SD Card Storage starting...\n");
    
    init_sd_card();
    load_ducky_script();
    
    // Initialize USB with device mode
    tud_init(BOARD_TUD_RHPORT);
    
    while (!tud_mounted()) {
        tud_task();
        sleep_ms(1);
    }
    
    printf("USB connected!\n");
    blink_led(2);
    
    sleep_ms(3000);
    if (script_loaded) {
        script_running = true;
        printf("Starting script execution...\n");
    }
    
    while (1) {
        tud_task();
        
        if (script_running) {
            process_ducky_script();
        }
        
        static uint32_t last_blink = 0;
        if (board_millis() - last_blink > 1000) {
            gpio_put(LED_PIN, !gpio_get(LED_PIN));
            last_blink = board_millis();
        }
    }
    
    return 0;
}
