# GPS Cookie

**An arduino compatible open-source GPS data logger that is ready to go.**

Designed by Richard Haberkern.

- [Website](http://gpscookie.com)
- [Kickstarter](https://www.kickstarter.com/projects/richardhaberkern/gps-cookie-leaving-crumbs-wherever-it-goes)

## Project Files

- [arduino](https://github.com/albertico/gps-cookie/tree/master/arduino) » Default Arduino sketch that powers the GPS Cookie
- [firmware](https://github.com/albertico/gps-cookie/tree/master/firmware) » ATmega328P HEX file
- [parts-list-bom](https://github.com/albertico/gps-cookie/tree/master/parts-list-bom) » Parts List (BOM)
- [schematics](https://github.com/albertico/gps-cookie/tree/master/schematics) » GPS Cookie schematics

## GPS Cookie Instructions

### Usage

1. Format a 2GB – 16GB Micro SD card in Windows (MSDOS-FAT).  _You can do this on a Windows or MAC based computer._
2. Insert the SD card into the GPS Cookie slot.
3. With 2 fresh AAA batteries installed, turn on the GPS Cookie using the slider switch.
4. The **red** led should start blinking every second if the SD card is installed and formatted **correctly**.
5. Take the Cookie outdoors and wait a few minutes for the GPS receiver to lock on to the satellites.  The led will start flashing **green** to let you know it is **recording**.
6. Go for a drive, walk or bike ride and record the locations of everywhere you go.
7. When you return, you can view the recorded data as a text file or open it directly in Google Earth via the _'tools'_, _'GPS'_ menu.  Select the import (import from file) and load the data directly into Google Earth.

### Firmware Update

#### Problem 1: GPS

The ATmega328P only has a single UART, so the GPS module is connected to the same serial port that is used for debug logging and bootloader communication. As soon as the GPS module is powered, it will start reporting its status at 9600 Baud on pin PD0, which is also the TX pin of the FTDI chip. This will confuse the bootloader, so the GPS module needs to be disabled for the firmware update to work. Therefore, connect pin PD2 to GND.

#### Problem 2: Reset

In addition to RX and TX the FTDI chip exports two more serial signals, DTR and RTS. Both are connected to /RESET, so they can be used to reset the ATmega328P. By default both signals are not configured identical, resulting in ~1V at /RESET and keeping the ATmega328P permanently in reset. You can change the setting in software to fix this, but apparently avrdude cannot use those signals to trigger resets automatically, so you'll have to do that manually anyway. The reset button has no effect when /RESET is under control of the FTDI chip, therefore do not connect this line.

#### Solution

Your final setup should look like this: Your GPS cookie is split in two halves, connected by three cables, PD0 and PD1 from J5 and one of the GND pins from J7. There is a fourth cable on the bottom half connecting PD2 from J5 to the other GND pin on J7.

1. Connect a mini USB cable to the top half.
2. Power on the bottom half.
3. Hold down the reset button.
4. Release the reset button and immediately start avrdude, e.g. with "avrdude -p m328p -c arduino -P /dev/ttyUSB0". This should print "avrdude: AVR device initialized and ready to accept instructions".

Use the same approach to update the firmware, using the Arduino GUI or avrdude directly.

## License

The GPS Cookie is an open source project.  
Feel free to modify the design and use it any way you wish.  

## Github compilation

Sketch, firmware, schematics and instructions compiled by **[Alberto A. Colón Viera](https://github.com/albertico)**.
