# Floppy 80 (M1) User Guide

## Configuration

An SD Card (FAT formatted) is required to contain the disk images, and configuration.

Configuration of Floppy90 is performed with the placement of
files on the SD-Card inserted in its card reader.

Files are placed in the "SYS", folder.

### SYS/HARDWARE.cfg

This file contains global configuration settings. The file and settings are
optional, and all have meaningful defaults The settings are:
* WAIT - Used to enable / disable wait states 1 = enabled; 0 = disabled (default)

e.g.
```
# PICO Virtual RAM $3000-$37EF
EnablePicoRAM=1

# Floppy Boot Block, used for normal booting.
EnableFDCBoot=1

# FreHD Boot Block, if using FreHD auto-boot
# Note: Generally floppy boot is preferred
EnableFreHDBoot=0

# Enable 32 KB Hi RAM, if running on 16KB
EnableHighRAM=0

# Enable use of wait states for fast CPU's
EnableWaitStates=0

#Enable the Generation of 40Hz interrupts
EnableInterrupts=1

# SD Card SPI Clock speed in Khz. (1000-25000)  
SDKHZ=5000
```

### Pico RAM

An important aspect of this system is ability to provide 256KB of banked RAM.
This is achieved by using the RP2350's internal RAM and exposing this to the Z80 CPU
in a banked manner at location `$3000-$37FF`.

For more information See the [Virtual Memory](VIRTUAL.md) section.

### Floppy Boot Block

Enabling this feature will allow the Floppy90 to expose a boot block
compatible with Model 1 Floppy Drive Controllers

For this feature to work a Binary boot block of 256 bytes must be provided
on the SD card, in a file called `/SYS/BOOT.SYS`

When a Level 2 machine boots this block will be serviced by the Floppy90
and the boot process will execute the code at address `$4200`

### FreHD Boot Block

Enabling this feature will allow the Floppy90 to expose a boot block
compatible with the FreHD auto Boot Rom. The boot block provided is
preconfigured and allows the next stage (typically) of a FreHD boot.

The boot block will look for a file called `FREHD.ROM` (in root directory)
load and execute this.

### High RAM

Used to enable / disable 32KB Hi Ram RAM at address $8000 to $FFFF

### Wait States

Wait states are used to slow the Z80 CPU, to get it to wait for all
floppy disk controller actions to complete before continuing.
Typically, wait states should not be required.

Wait states are useful for overclocked CPU's where the CPU outperforms
the Floppy emulation, which is optimised for normal CPU speed.
The issue with wait states is they are known to disrupt
critical timed operations, such as formatting a floppy disk.

### Interrupts

Enables the generation of 40Hz interrupts.

### SDKHZ

SD card SPI clock in kHz, e.g. SDKHZ=12500. The card is always mounted and system.cfg read at the
firmware's boot speed (5000 by default); the clock is then changed to this value. Accepted range is 1000 to 25000.
Not all cards work at higher speeds (a card that is too slow for the setting can make the Floppy80 lock up), so
increase it in steps, e.g. 8333, 10000, 12500, and test with your card. If it is missing, the boot speed is kept

## File Services API

The primary purposes of Floppy90 is the provision of a Fileservices API
compatible with the FreHD, but an extension is provided to allow for multiple
files to be open at once.

-tbc-

## Debugging

The Firmware provides a debugging interface for logging and diagnosing issues.

While running the Floppy 90 provides a Command Line interface via serial
over the USB port on the Pi PICO. Simply connect a USB can to a host computer and
connect a Serial Communications program to the serial port

While the port may appear differently on different operating systems on a Mac
it appears something like `dev/cu.usbmodem000000`. The port is not a true serial port
and has no explicit Baud Rate, etc.

Pressing enter should display a help menu explaining the basic commands.

| COMMAND | DESCRIPTION                                    |
|---------|------------------------------------------------|
| help    | Display CLI Help screen                        |
| status  | Display Status                                 |
| dir f   | Display a Directory (optional filter)          |
| flash   | Put Pico into Flash mode to allow new firmware |
| reboot  | Reboot the PICO                                |
| test    | run some internal UNIT tests                   |


