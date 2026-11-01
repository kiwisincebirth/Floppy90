# Floppy 90

## Background

Floppy-90 is a Software solution that runs on the Raspberry Pi RP2350 
microcontroller, that exposes services to software running on a Zilog Z80

The target Platform for this is the TRS-80 Model 1 (and compatible) computers,
and is based on the Floppy-80 (Model 1) hardware device.
This device provides an interface to the Z80 bus so services can be used 
by software running on the Z80 cpu.

It is currently exclusively used by the kDOS operating system which utilises
it for is backend file storage.

## Features

Floppy-90 is a Software solution that provides  
- A basic File Services API compatible with FreHD
- Extended File Services API allowing for multiple files to be held open
- Ability to serve a Floppy drive boot block
- Ability to serve a FreHD boot sector
- 256k of Banked RAM in unused 3000-37FF memory addresses 
- 32k RAM in high memory space (optional)
- 40Hz RTC interrupt generation (optional)
- Wait State Generation (optional)
- Software Debugging Interface

Unlike the Floppy-80 project (which this is based) neither Floppy of Hard Drive
emulation is provided in any meaningful way.

## Documentation

TODO

## Software Build

You need to install the C SDK for RPI
https://www.raspberrypi.com/documentation/microcontrollers/c_sdk.html#sdk-setup

CMake and Make are essential for this to work
* On Mac OSX - `brew install cmake`

```
# ensure you are in the firmware directory
cd firmware

# first run cmake - creating a separate build folder 
cmake -B build -S .

# then from the build folder
cd build

# run the make command
make Floppy90
```

This will create Floppy90.uf2 file which you flash to the RPI PICO

Alternately you can use VSCode and the RPI Pico extension.

### Background Reading

These are DEV resources:
* https://www.raspberrypi.com/documentation/microcontrollers/c_sdk.html
* https://admantium.medium.com/getting-started-with-raspberry-pico-and-cmake-f536e18512e6

## Credits

Floppy 80 Model 1 Project which this software is based.
