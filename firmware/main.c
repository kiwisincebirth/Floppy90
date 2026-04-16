#include <stdio.h>
#include <string.h>
#include <hardware/vreg.h>

#include "pico/stdlib.h"
#include "pico/multicore.h"
#include "hardware/gpio.h"

#include "hardware/pio.h"
#include "hardware/irq.h"
#include "hardware/regs/intctrl.h"
#include "hardware/clocks.h"
#include "hardware/structs/systick.h"

#include "defines.h"
#include "sd_core.h"
#include "fdc.h"
#include "hdc.h"
#include "frehd.h"
#include "system.h"
#include "cli.h"
#include "z80_bus.h"
#include "memory.h"
#include "file.h"

///////////////////////////////////////////////////////////////////////////////
// API documentions is located at
// https://raspberrypi.github.io/pico-sdk-doxygen/
///////////////////////////////////////////////////////////////////////////////

///////////////////////////////////////////////////////////////////////////////
// to get memory usage of the compilied program enter
//
//   size build/Floppy80.elf
//
// in a bash terminal window
//
// Output is
// - text gives you the size of the text segment (or code segment).
// - data gives you the size of the data segment.
// - bss gives you the size of the block started by symbol segment.
// - dec is the size of the text, data and bss size added together in decimal, and hex is the same number in hexadecimal.
///////////////////////////////////////////////////////////////////////////////

///////////////////////////////////////////////////////////////////////////////
// GPIO NET        IN   OUT
//-----------------------------------------------------------------------------
// 0	INT         	 * (drives a transistor to pull the Z80 Interrupt line low)
// 1    WAIT             * (drives a transistor to pull the Z80 Wait line low)
// 2    CLK_SCLK         * (SD-Card)
// 3    CMD_MOSI         * (SD-Card)
//
// 4    DAT0_MISO   *      (SD-Card)
// 5    DAT3_CS          * (SD-Card)
// 6    CD               * (SD-Card, Card Detect)
// 7    ADDRL_OE         * (Enable the low (A0-A7) byte address buffer for reading)
//
// 8    DATAB_OE         * (Enable the data bus for reading)
// 9    IN          *      (Z80 bus for port I/O, input)
// 10   ADDRH_OE         * (Enable the high (A8-A15) byte address buffer for reading)
// 11   RD          *      (Z80 bus for memory read)
//
// 12   WR          *      (Z80 bus for memory write)
// 13   OUT         *      (Z80 bus for port I/O, output)
// 14   LED              * (active low enable for the activity LED)
// 15   INT              * (drives a transistor to pull the Z80 interrupt line low)
//
// 16   D0          *    * (bit 0 of the data bus to read A0-A7, A8-A15 and D0-D7, and wite D0-D7)
// 17   D1          *    * (bit 1 of the data bus)
// 18   D2          *    * (bit 2 of the data bus)
// 19   D3          *    * (bit 3 of the data bus)
//
// 20   D4          *    * (bit 4 of the data bus)
// 21   D5          *    * (bit 5 of the data bus)
// 22   D6          *    * (bit 6 of the data bus)
// 23   NA
//
// 24   NA
// 25   NA
// 26   D7          *    * (bit 7 of the data bus)
// 27   DIR              * (controls the direction of the data bus buffer)
//
// 28   SYSRES      *      (Z80 bus reset state)
// 29	-
// 30	-
// 31	-
//
//-----------------------------------------------------------------------------
void InitGPIO(void)
{
    gpio_init(INT_PIN);
    gpio_set_dir(INT_PIN, GPIO_OUT);
    gpio_set_slew_rate(INT_PIN, GPIO_SLEW_RATE_FAST);
    gpio_set_drive_strength(INT_PIN, GPIO_DRIVE_STRENGTH_2MA);
    gpio_put(INT_PIN, 0); // deactivate intr

    gpio_init(WAIT_PIN);
    gpio_set_dir(WAIT_PIN, GPIO_OUT);
    gpio_set_slew_rate(WAIT_PIN, GPIO_SLEW_RATE_FAST);
    gpio_set_drive_strength(WAIT_PIN, GPIO_DRIVE_STRENGTH_2MA);
    gpio_put(WAIT_PIN, 0); // deactivate wait

    // CLK_SCLK - initailized by the SD-Card library
    // CMD_MOSI
    // DAT0_MISO
    // DAT3_CS

    gpio_init(LED_PIN);
    gpio_set_dir(LED_PIN, GPIO_OUT);
    gpio_set_slew_rate(LED_PIN, GPIO_SLEW_RATE_SLOW);
    gpio_set_drive_strength(LED_PIN, GPIO_DRIVE_STRENGTH_2MA);
    gpio_put(LED_PIN, 1); // LED off

    gpio_init(ADDRL_OE_PIN);
    gpio_set_dir(ADDRL_OE_PIN, GPIO_OUT);
    gpio_set_slew_rate(ADDRL_OE_PIN, GPIO_SLEW_RATE_FAST);
    gpio_set_drive_strength(ADDRL_OE_PIN, GPIO_DRIVE_STRENGTH_12MA);
    gpio_put(ADDRL_OE_PIN, 1); // deactivate low address bus input buffer

    gpio_init(DATAB_OE_PIN);
    gpio_set_dir(DATAB_OE_PIN, GPIO_OUT);
    gpio_set_slew_rate(DATAB_OE_PIN, GPIO_SLEW_RATE_FAST);
    gpio_set_drive_strength(DATAB_OE_PIN, GPIO_DRIVE_STRENGTH_12MA);
    gpio_put(DATAB_OE_PIN, 1); // deactivate data bus I/O buffer

    gpio_init(ADDRH_OE_PIN);
    gpio_set_dir(ADDRH_OE_PIN, GPIO_OUT);
    gpio_set_slew_rate(ADDRH_OE_PIN, GPIO_SLEW_RATE_FAST);
    gpio_set_drive_strength(ADDRH_OE_PIN, GPIO_DRIVE_STRENGTH_12MA);
    gpio_put(ADDRH_OE_PIN, 1); // deactivate high address bus input buffer

    gpio_init(IN_PIN);
    gpio_set_dir(IN_PIN, GPIO_IN);

    gpio_init(RD_PIN);
    gpio_set_dir(RD_PIN, GPIO_IN);

    gpio_init(WR_PIN);
    gpio_set_dir(WR_PIN, GPIO_IN);

    gpio_init(OUT_PIN);
    gpio_set_dir(OUT_PIN, GPIO_IN);

    gpio_init(MREQ_PIN);
    gpio_set_dir(MREQ_PIN, GPIO_IN);

    gpio_init(D0_PIN);
    gpio_set_dir(D0_PIN, GPIO_IN);

    gpio_init(D1_PIN);
    gpio_set_dir(D1_PIN, GPIO_IN);

    gpio_init(D2_PIN);
    gpio_set_dir(D2_PIN, GPIO_IN);

    gpio_init(D3_PIN);
    gpio_set_dir(D3_PIN, GPIO_IN);

    gpio_init(D4_PIN);
    gpio_set_dir(D4_PIN, GPIO_IN);

    gpio_init(D5_PIN);
    gpio_set_dir(D5_PIN, GPIO_IN);

    gpio_init(D6_PIN);
    gpio_set_dir(D6_PIN, GPIO_IN);

    gpio_init(D7_PIN);
    gpio_set_dir(D7_PIN, GPIO_IN);

    gpio_init(DIR_PIN);
    gpio_set_dir(DIR_PIN, GPIO_OUT);
    gpio_set_slew_rate(DIR_PIN, GPIO_SLEW_RATE_FAST);
    gpio_put(DIR_PIN, 1); // A to B direction

    gpio_init(SYSRES_PIN);
    gpio_set_dir(SYSRES_PIN, GPIO_IN);

    gpio_init(CD_PIN);
    gpio_set_dir(CD_PIN, GPIO_IN);
    gpio_set_pulls(CD_PIN, true, false);
}

void __not_in_flash_func(ServiceBackendStateMachines)(void) {
	while (true) {

		ServiceCli();

		if ( ! g_byResetActive) {
			// When Not In Reset
			ServiceFreHdStateMachine();
			UpdateCounters(); // todo ? move this code above the if
		}
	}
}

///////////////////////////////////////////////////////////////////////////////
int main()
{

	// !!!!! Completely Experimental !!!!!!!!!!!
	// !!!!! Completely Experimental !!!!!!!!!!!
	// !!!!! Completely Experimental !!!!!!!!!!!

	// vreg_set_voltage(VREG_VOLTAGE_1_05); // Max of 260Mhz
	vreg_set_voltage(VREG_VOLTAGE_1_10); // Max of 300Mhz
	// vreg_set_voltage(VREG_VOLTAGE_1_15); // Max of 330Mhz
	// set_sys_clock_khz(240000,true);
	// set_sys_clock_khz(250000,true);
	set_sys_clock_khz(260000,true);
	// set_sys_clock_khz(270000,true);
	// set_sys_clock_khz(280000,true);
	// Below 300 Mhz if running Flash

    stdio_init_all();

    InitGlobalVars();	// general, RTC / Reset State
    InitGPIO();			// GPIO Pin Config
    SDHC_Init();		// Init SD Card Interface
	MountSdCard();		// Mount the SD Card Filesystem
	// TestSdCardInsertion();
    FileSystemInit();	// Init Filesystem Buffers.
    SysInit();			// Init State - Hardware.CFG File
	MemInit();			// init Paged Memory State
	LoadFDCBootBlock(); // Loads BOOT.SYS - FDC boot block
	FreHDInit();		// currently does nothing
    InitCli();			// init some variables

	// service Memory Requests
    // multicore_launch_core1(service_memory);
    multicore_launch_core1(ServiceZ80BusRequest);

	// Core 0 Worker functionality
    ServiceBackendStateMachines();
}
