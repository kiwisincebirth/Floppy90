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
#include "ports.h"
#include "system.h"
#include "cli.h"
#include "z80_bus.h"
#include "memory.h"
#include "file.h"

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
    gpio_init(LED_PIN);
    gpio_set_dir(LED_PIN, GPIO_OUT);
    gpio_set_slew_rate(LED_PIN, GPIO_SLEW_RATE_SLOW);
    gpio_set_drive_strength(LED_PIN, GPIO_DRIVE_STRENGTH_2MA);
    gpio_put(LED_PIN, 1); // LED off

    gpio_init(CD_PIN);
    gpio_set_dir(CD_PIN, GPIO_IN);
    gpio_set_pulls(CD_PIN, true, false);
}

void __not_in_flash_func(ServiceBackendStateMachines)(void) {
	while (true) {

		ServiceCli();

		if ( ! g_byResetActive) {
			// When Not In Reset
			ServicePortsStateMachine();
			UpdateCounters(); // todo ? move this code above the if
		}
	}
}

///////////////////////////////////////////////////////////////////////////////
int main()
{
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
    InitGPIO();			// GPIO Pin Config, general pins
	InitZ80BusGPIO();	// Z80 Pin initialisation
    SDHC_Init();		// Init SD Card Interface, including pins
	MountSdCard();		// Mount the SD Card Filesystem
	// TestSdCardInsertion();
    FileSystemInit();	// Init Filesystem Buffers.
    SysInit();			// Init State - Hardware.CFG File
	MemInit();			// init Paged Memory State
	LoadFDCBootBlock(); // Loads BOOT.SYS - FDC boot block
	PortsInit();		// currently does nothing
    InitCli();			// init some variables

	// service Memory Requests
    // multicore_launch_core1(service_memory);
    multicore_launch_core1(ServiceZ80BusRequest);

	// Core 0 Worker functionality
    ServiceBackendStateMachines();
}
