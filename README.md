# Floppy 90

## Background

The Floppy-90 is a Software solution that runs on the Floppy-80
(Model 1) hardware and provides 

It is based on the FreHD API's, but extends these to provide additional features

It is currently exclusivly used by the kDOS operating system which utilises
it for is backend file storage.

## Features

The Floppy-90 is a Software solution that emulates 
- Basic File Services API compatible with FreHD
- Extended File Services API allowing for multiple files to be held open
- Ability to serve a Floppy drive boot block
- Ability to serve a FreHD boot sector
- 256k of Banked RAM in unused 3000-37FF memory addresses 
- 32k RAM in high memory space (optional)
- 40Hz RTC interrupt generation (optional)
- Wait State Generation (optional)
- Software Debugging Interface

An SD card is required to store disk images and configuration data

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

This will create Floppy90.uf2 file which you burn to the RPI PICO

Alternately you can use VSCode and the RPI Pico extension.

### Background Reading

These are DEV resources:
* https://www.raspberrypi.com/documentation/microcontrollers/c_sdk.html
* https://admantium.medium.com/getting-started-with-raspberry-pico-and-cmake-f536e18512e6

## Credits

Floppy 80 Model 1 Project
