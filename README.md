# Floppy 80 for the TRS-80 Model I

## Background

FatFS
https://elm-chan.org/fsw/ff/


Based off
https://github.com/carlk3/no-OS-FatFS-SD-SPI-RPi-Pico

This repository is more or less frozen. 
Exceptions will be made for bug fixes and compatibility 
with new releases of the Pico SDK and FatFS. 
Any new development will take place in the new repository: 
no-OS-FatFS-SD-SDIO-SPI-RPi-Pico.
This repository supports SPI-attached SD cards only. 
The new repository supports SPI- and/or SDIO-attached SD cards.

which includes
FatFs module 
R0.15 (November 6, 2022)

### FreHD

Includes a VERY OLD ff
FatFs - FAT file system module include file  
R0.09b    (C)ChaN, 2013
Jan 24,'13 R0.09b Added f_setlabel() and f_getlabel(). (_USE_LABEL = 1)

## Features

The Floppy-80 is a Hardware and software solution that emulates 
the following features of the Expansion Interface
- A Floppy Disc controller
  - supports 3 floppy drives
  - single and double-density
  - single and double-sided
  - up to 90 cylinders
  - DMK disk format supported
- 32k RAM expansion (optional)
- 40Hz RTC interrupt generation

Additionally, the following features are provided:
- A Hard Disk Controller (optional)
  - supports up to 2 hard drives
  - Uses same image format as FreHD
- Wait State Generation (optional)
- Software Debugging Interface

An SD card is required to store disk images and configuration data

## Hardware

The Floppy-80 consists of a board that connects to the Model-1 expansion edge connector.
The board contains an SD card socket to provide disk images and a
Raspberry Pi Pico 2 microcontroller which does the emulation

[Building and testing](HARDWARE.md) the board is covered separately.

## Software

The software provided includes 2 main components
* Firmware running on Raspberry Pi, that provides the emulation
* FDC Utility (TRS DOS, and CPM) running on TRS-80 for emulator control

See the separate [Software Build Guide](BUILDING.md) for details on 
building the Floppy-80 software

## User Guide

For User Instructions please see the separate [User Guide](USER-GUIDE.md)

## Credits

## Resources


