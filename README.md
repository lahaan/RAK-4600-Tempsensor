# RAK4600EVB Programming Guide

This guide covers the setup and programming process for the RAK4600(H) WisDuo Evaluation Board using the Arduino IDE with ST-Link V2. It is worth nothing that there is a version called RAK4601, but that version is identical to the "normal" 4600. It is just tailored towards the Chinese market and bands. This build does not use official firmware, APIs nor hardware recommended by RAKWireless, as it was not an option for us. For a RUI-based project, you can have a look at this project here (albeit STM32 based):
https://github.com/lahaan/loratempmodule


## Required Hardware

### Official Options
- RAKWireless RAKDAP
- SEGGER J-Link
- nRF Connect app (via DFU for code updates)

### Used in this project
- **CP21xx-series UART-USB module** (note: UART is not available initially with official firmware)
- **ST-Link V2** (recommended for programming). 
It is possible flash firmware using a mobile device with the nRF Connect application (using Bluetooth), however, since the lack of Segger J-Link and the time it would take to flash firmware, this option was disregarded. 

### Power Considerations
- The board accepts up to 3.6V
- Our batteries (3.5V-3.55V in series) are at the edge of the limit but functional
- UART logic voltage is directly tied to input voltage

## Development Environment Setup

### 1. Install Arduino IDE
Use a recent version of Arduino IDE.

### 2. Add Board Support
Add the following URL to Arduino IDE Preferences:
```
https://sandeepmistry.github.io/arduino-nRF5/package_nRF5_boards_index.json
```

### 3. Install Board Package
From the Boards Manager, install **"Nordic Semiconductor nRF5 Boards"**.

We use this instead of the RAKWireless nRF52 package because the RAKWireless version does not support ST-Link.

### 4. Modify Platform Configuration

Locate the following directory:
```
AppData\Local\Arduino15\packages\sandeepmistry\hardware\nRF5\0.8.0\
```

#### 4.1 Modify `platform.txt`

Find the line starting with `compiler.c.elf.flags=` and change it to:
```
compiler.c.elf.flags={compiler.warning_flags} -Os -Wl,--gc-sections -save-temps -Wl,--no-warn-mismatch
```

This prevents VFP register-related compilation errors.

#### 4.2 Modify Compiler Warning Flags

In the same `platform.txt` file, update the warning flags to:
```
compiler.warning_flags=-w 
compiler.warning_flags.none=-w 
compiler.warning_flags.default= 
compiler.warning_flags.more=-Wall 
compiler.warning_flags.all=-Wall -Wextra
```

### 5. Modify `ArduinoHal.cpp`

Locate `ArduinoHal.cpp` in the board package and comment out the following functions:
```cpp
// ::tone(pin, frequency, duration);
// ::noTone(pin);
```

### 6. Restart Arduino IDE

After making all modifications, restart the Arduino IDE.

## Arduino IDE Settings

Use the following configuration:

- **Board:** Generic nRF52
- **Crystal Oscillator**
- **Softdevice:** None
- **Programmer:** ST-Link V2

### Try compiling code
If any issues come up, clear the Arduino cache. **Warnings are okay**

## Hardware Connections

### Programming with ST-Link V2

Connect the following pins directly to the RAK4600(H):

- **SWDIO** - SWD Data
- **SWCLK** - SWD Clock
- **3.3V** - Power
- **GND** - Ground

**Important:** When programming with ST-Link V2, disconnect the RX/TX pins from any USB-to-UART module. Leaving them connected can create unintended power paths and interfere with programming.

### Burn the bootloader

Try burning the bootlader. This will clear the chip. 

### UART Debug Connection (Optional)

For serial debugging via RX/TX:

- **RX** - P0.18 (J10)
- **TX** - P0.19 (J10)
- **3.3V** - Power (prefer to power via UART-to-serial module when using UART)
- **GND** - Ground

**Note:** UART is not available with the original RAK firmware.

### Pin Definitions

- **IO1 (J11):** P0.11
- **RX/TX (J10):** P0.18/P0.19

## Compilation Warnings

The following warnings during compilation are normal and can be ignored:

```
ld.exe: warning: changing start of section .bss by 4 bytes
ld.exe: warning: changing start of section .heap by 4 bytes
ld.exe: warning: changing start of section .stack_dummy by 4 bytes
```

Expected memory usage:
```
Sketch uses 56160 bytes (10%) of program storage space. Maximum is 524288 bytes.
```

## Example Code with Low-Power Sleep

This repo has an example that demonstrates LoRaWAN temperature sensing with System ON sleep mode using RTC2 for timing.


## Note

### ABI Compatibility
```cpp
  // Safe integer printing to avoid ABI mismatch issues
  int tempInt = (int)temp;
  int tempDec = abs((int)(temp * 100) % 100);
```
Safe ABI handling is critical. Without proper integer handling, software and hardware FPU mismatches will occur, preventing compilation. This is due to the legacy board library (the only one supporting ST-Link). While it's theoretically possible to modify assembly-level settings, this would cause conflicts such as stuck states from overly precise temperature readings.


## References

- [RAK4600 WisDuo Evaluation Board Quick Start Guide](https://docs.rakwireless.com/Product-Categories/WisDuo/RAK4600-Evaluation-Board/Quickstart/)
- [RAK4600 Module Quick Start Guide](https://docs.rakwireless.com/Product-Categories/WisDuo/RAK4600-Module/Quickstart/)

## TTN Integration

For The Things Network integration, refer to the RAK4600 Module Quick Start Guide for detailed instructions on registering your device and configuring network settings.
