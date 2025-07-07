# RP2040 Rubber Ducky with SD Card Storage

A comprehensive Rubber Ducky implementation for the Raspberry Pi RP2040 microcontroller that combines HID keystroke injection with mass storage functionality via SD card.

##  Features

- **Dual USB Functionality**: Appears as both HID keyboard and USB mass storage device
- **SD Card Integration**: Ducky scripts stored on removable SD card for easy modification
- **Flexible Script System**: Supports standard Rubber Ducky script commands
- **Mass Storage**: SD card contents accessible as USB drive for file transfer
- **Error Handling**: Graceful fallback when SD card is missing (uses internal default script)
- **Debug Output**: Serial console for troubleshooting via USB CDC
- **Customizable**: Adjustable typing speed and pin assignments

##  Hardware Requirements

- Raspberry Pi Pico or compatible RP2040 board
- MicroSD card module (SPI interface)
- MicroSD card (FAT32 formatted)
- Jumper wires for connections

##  Wiring Diagram

```
SD Card Module    RP2040 GPIO    Physical Pin
VCC           →   3.3V       →   Pin 36
GND           →   GND        →   Pin 38  
MISO          →   GPIO 4     →   Pin 6
MOSI          →   GPIO 3     →   Pin 5
SCK           →   GPIO 2     →   Pin 4
CS            →   GPIO 5     →   Pin 7
```

##  Software Requirements

### Ubuntu/Debian:
```bash
sudo apt update
sudo apt install cmake gcc-arm-none-eabi libnewlib-arm-none-eabi build-essential git python3
```

### macOS:
```bash
brew install cmake git python3
brew install --cask gcc-arm-embedded
```

### Windows:
- [CMake](https://cmake.org/download/)
- [ARM GCC Toolchain](https://developer.arm.com/tools-and-software/open-source-software/developer-tools/gnu-toolchain/gnu-rm/downloads)
- [Git](https://git-scm.com/downloads)

##  Quick Start

### 1. Clone and Setup

```bash
git clone https://github.com/YOUR_USERNAME/rp2040-rubber-ducky-storage.git
cd rp2040-rubber-ducky-storage

# Initialize submodules
git submodule update --init --recursive
```

### 2. Build

```bash
# Set environment variable
export PICO_SDK_PATH="$(pwd)/lib/pico-sdk"

# Configure and build
mkdir -p build && cd build
cmake ..
make -j$(nproc)
```

### 3. Flash Firmware

1. Hold BOOTSEL button on RP2040
2. Connect USB cable
3. Release BOOTSEL button
4. Copy firmware:
   ```bash
   cp rp2040_rubber_ducky.uf2 /media/$USER/RPI-RP2/
   ```

### 4. Prepare SD Card

Format SD card as FAT32 and create `ducky.txt`:

```bash
# Example ducky script
cat > /path/to/sdcard/ducky.txt << 'EOF'
REM Example Pico Ducky Script
DELAY 2000
GUI r
DELAY 500
STRING notepad
ENTER
DELAY 1000
STRING Hello from RP2040 Rubber Ducky!
ENTER
STRING This device also provides mass storage!
ENTER
STRING Check the USB drive that appeared.
EOF
```

##  Supported Ducky Commands

| Command | Description | Example |
|---------|-------------|---------|
| `DELAY` | Pause execution (milliseconds) | `DELAY 1000` |
| `STRING` | Type text | `STRING Hello World` |
| `ENTER` | Press Enter key | `ENTER` |
| `SPACE` | Press Space key | `SPACE` |
| `TAB` | Press Tab key | `TAB` |
| `ESCAPE` | Press Escape key | `ESCAPE` |
| `GUI` | Windows/Super key + key | `GUI r` |
| `CTRL` | Control key combinations | `CTRL c` |
| `ALT` | Alt key combinations | `ALT F4` |
| `SHIFT` | Shift key combinations | `SHIFT TAB` |

##  Customization

### Change Pin Assignments

Edit `src/main.c`:
```c
const uint SD_PIN_MISO = 4;    // Change these values
const uint SD_PIN_CS   = 5;
const uint SD_PIN_SCK  = 2;
const uint SD_PIN_MOSI = 3;
```

### Modify Default Typing Speed

Edit `src/main.c`:
```c
static uint32_t key_delay = 50; // Change delay in milliseconds
```

### Add Custom Commands

Extend `parse_ducky_command()` function in `src/main.c`:
```c
else if (strcmp(line, "CUSTOM_CMD") == 0) {
    // Your custom implementation
}
```

##  Troubleshooting

### Common Issues

**SD Card Not Detected:**
- Check wiring connections
- Ensure SD card is formatted as FAT32
- Try a different SD card

**Script Not Running:**
- Verify `ducky.txt` exists on SD card
- Check script syntax
- Device waits 3 seconds before execution

**Build Errors:**
- Ensure all dependencies are installed
- Verify Pico SDK path is correct
- Check that submodules are initialized

**USB Not Recognized:**
- Try different USB cable
- Check Device Manager (Windows) or `lsusb` (Linux)
- Verify firmware was flashed correctly

### Debug Output

Connect to serial port for debug information:

```bash
# Linux/macOS
screen /dev/ttyACM0 115200

# Windows - use PuTTY with COMx port at 115200 baud
```

##  Project Structure

```
rp2040-rubber-ducky-storage/
├── src/
│   ├── main.c              # Main application
│   ├── sd_card.c           # SD card driver
│   ├── sd_card.h           # SD card header
│   ├── diskio.c            # FatFs disk I/O
│   ├── tusb_config.h       # TinyUSB configuration
│   └── ffconf.h            # FatFs configuration
├── lib/
│   ├── pico-sdk/           # Pico SDK (submodule)
│   ├── tinyusb/            # TinyUSB library (submodule)
│   └── fatfs/              # FatFs library
├── build/                  # Build output
├── CMakeLists.txt          # Build configuration
├── pico_sdk_import.cmake   # SDK import
└── README.md               # This file
```

##  Contributing

Contributions are welcome! Please feel free to submit a Pull Request. For major changes, please open an issue first to discuss what you would like to change.

### Development Setup

1. Fork the repository
2. Create a feature branch (`git checkout -b feature/amazing-feature`)
3. Commit your changes (`git commit -m 'Add amazing feature'`)
4. Push to the branch (`git push origin feature/amazing-feature`)
5. Open a Pull Request

##  License

This project is licensed under the MIT License - see the [LICENSE](LICENSE) file for details.

##  Legal Notice

**IMPORTANT:** This tool is for educational and authorized testing purposes only. 

-  Use only on systems you own or have explicit written permission to test
-  Never use on systems without proper authorization
-  Ensure compliance with all applicable laws and regulations

Unauthorized use of keystroke injection devices may violate computer fraud and abuse laws in your jurisdiction. Always use responsibly and ethically.

## Acknowledgments

- [Raspberry Pi Foundation](https://www.raspberrypi.org/) for the excellent RP2040 microcontroller
- [TinyUSB](https://github.com/hathach/tinyusb) for USB stack implementation
- [FatFs](http://elm-chan.org/fsw/ff/) for FAT filesystem support
- [Pico SDK](https://github.com/raspberrypi/pico-sdk) for development framework
- Original [Rubber Ducky](https://github.com/hak5darren/USB-Rubber-Ducky) project for inspiration

