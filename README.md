# z80-mbc2-esp32
A Z80 CPU board with an ESP32 as SPI slave peripheral

The purpose of this project is to connect an ESP32 module to a Z80 system. ESP32 module highlights: 32bit CPU, bluetooth and wifi connectivity.
The project is based around the well knowed Z80-MBC2 board https://j4f.info/z80-mbc2. In the original project, the SD_MOD slot allow the connection of an SD card module, effectively it's the SPI port of the ATMEGA32 uC. This project use this port to connect the ATMEGA32A (SPI master) to the HSPI port of an ESP32 board (SPI slave). The SD card module is moved to a second SPI port of the ESP32.

This project is still is in a very early stage!

TODO
- Write the arduino library for the ATMEGA32 (ESP32 as peripheral API)
- Add new instructions to the Nascom Basic
- Write native Z80 intenet clients

DOING
- Write the ESP32 firmware: make the SD card, bluetooth and wifi functions configurable and available through the SPI port.
- Modify the original IOS ATMEGA32 firmware.

DONE
- ESP32 interface schematic
- ESP32SPISlave Arduino library bug fix (the original doesn't work, use the library of this repo).
- ESP32 test firmware (on spi command, set pin G22 LED on/off).
- Added a new opcode to the IOS-lite to test ESP firmware (new opcode 0x10).
- Define an SPI protocol for the data transfer so the ESP32 and ATMEGA32 can understand each other.
- Created SD card to SPI functions for the ESP32 (remotize the SD card).
- Modified IOS SD card functions, they do SD card request over SPI at the ESP32 (remotize the SD card).
