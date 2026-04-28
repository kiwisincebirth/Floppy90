//
// Created by Mark Pruden on 29/3/2026.
//

#include <hardware/sync.h>
#include <hardware/structs/sio.h>

#include "defines.h"
#include "frehd.h"
#include "memory.h"
#include "z80_bus.h"

#include <hardware/gpio.h>


void InitZ80BusGPIO(void)
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
}


void __not_in_flash_func(ServiceZ80BusRequest)(void)
{

    register word bus;
    register word addr;

    while (1)
    {
        while (!get_gpio(SYSRES_PIN)) {
           	g_byResetActive = true;
        }
       	g_byResetActive = false;

        if (g_byRaiseInterrupt)
        {
            g_byRaiseInterrupt = false;
        	set_gpio(INT_PIN); // activate intr
        }

        // turn bus to input from Z80
        set_bus_as_input();     // reset data pins (D0-D7) as inputs
        set_gpio(DATAB_OE_PIN); // disable data bus transceiver
        set_gpio(ADDRL_OE_PIN);	// disable  address Bus Low
        clr_gpio(ADDRH_OE_PIN); // enable address Bus High

        // wait for ONE of MREQ, RD, WR or IN, OUT to go active(low),
        // reading LOW address byte at the same time
        // as address is put on bus before MREQ IORQ
        do {
            bus = get_gpio_read_bus();
        } while ((bus & 0x1F) == 0x1F);

		// The high order address $00xx - $FFxx
        addr = (bus << (8 + IN_PIN - D0_PIN)) & 0xFF00;

		// if it is a memory request
		if ( ! (bus & 0x10) ) { // MREQ pin is active

			byte highOrder = g_byEnableUpperMem && (addr >= 0x8000);
			byte extraMem = (addr >= 0x3000) && (addr < 0x3800);

			// first check the address is serviceable
			if (extraMem || highOrder ) {

				// address lookup in range so add wait state
				//f (g_byEnableWaitStates) set_gpio(WAIT_PIN);

				// flip to enable low order address as input
				set_gpio(ADDRH_OE_PIN);
				clr_gpio(ADDRL_OE_PIN);

				NopDelay(); // propagation delay

				// read and combine low address byte 0x0000 - 0xFFFF
				addr = addr + (get_gpio_data_byte() & 0xFF);
				byte bIsRead = ! get_gpio(RD_PIN); // READ (or write) BUS operation
				// todo is read could be from bus & 0xXX

				if (highOrder) {
					ServiceHighMemoryOperation(addr, bIsRead); // > 8000
				} else {
					if (addr >= 0x37E0) {
						ServiceExtraPorts(addr, bIsRead); // 37E0 - 3800
					} else {
						ServiceExtraMemory(addr, bIsRead); // 3000 - 37E0
					}
				}
			}

		} else { // Assume it is a IO Request

			// flip to enable low order address as input
			set_gpio(ADDRH_OE_PIN);
			clr_gpio(ADDRL_OE_PIN);

			NopDelay();	// propagation delay

			// read low order address byte 0x00 - 0xFF
			addr = (get_gpio_data_byte() & 0xFF);

			if ( (addr >= 0xC0) && (addr < 0xD0) ) {

				// IO address in range so servicing Request
				//if (g_byEnableWaitStates) set_gpio(WAIT_PIN);

				if (!(bus & 0x01)) { // low on iORQ IN
					FinishReadOperation(frehd_port_in(addr));

				} else if (!(bus & 0x08)) { // low on iORQ OUT

					set_gpio(WAIT_PIN);
					frehd_port_out(addr, FinishWriteOperation());
				}
			}
		}

    	// remove any waits
		clr_gpio(WAIT_PIN);

		// wait for ALL of MREQ, IN, RD, WR and OUT to go inactive(HIGH)
		// this is after the last bus operation
		do {
			bus = get_gpio_read_bus();
		} while ((bus & 0x1F) != 0x1F);

    }

}

