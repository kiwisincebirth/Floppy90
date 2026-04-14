//
// Created by Mark Pruden on 29/3/2026.
//

#include <hardware/sync.h>
#include <hardware/structs/sio.h>

#include "defines.h"
#include "frehd.h"
#include "memory.h"
#include "z80_bus.h"

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
				// if (g_byEnableWaitStates) set_gpio(WAIT_PIN);

				// flip to enable low order address as input
				set_gpio(ADDRH_OE_PIN);
				clr_gpio(ADDRL_OE_PIN);

				NopDelay(); // propagation delay

				// read and combine low address byte 0x0000 - 0xFFFF
				addr = addr + (get_gpio_data_byte() & 0xFF);

				if (highOrder) {
					ServiceHighMemoryOperation(addr); // > 8000
				} else {
					if (addr >= 0x37E0) {
						ServiceExtraPorts(addr); // 37E0 - 3800
					} else {
						ServiceExtraMemory(addr); // 3000 - 37E0
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
				// if (g_byEnableWaitStates) set_gpio(WAIT_PIN);

				if (!(bus & 0x01)) { // low on iORQ IN
					FinishReadOperation(frehd_port_in(addr));

				} else if (!(bus & 0x08)) { // low on iORQ OUT
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


// void __not_in_flash_func(ServiceZ80BusRequest)(void)
// {
//
//     register word bus;
//     register word addr;
//
//     while (1)
//     {
//         while (!get_gpio(SYSRES_PIN)) {
//            	g_byResetActive = true;
//         }
//        	g_byResetActive = false;
//
//         if (g_byRaiseInterrupt)
//         {
//             g_byRaiseInterrupt = false;
//         	set_gpio(INT_PIN); // activate intr
//         }
//
//         // turn bus to input from Z80
//         set_bus_as_input();     // reset data pins (D0-D7) as inputs
//         set_gpio(DATAB_OE_PIN); // disable data bus transceiver
//         set_gpio(ADDRH_OE_PIN); // disable address Bus High
//         clr_gpio(ADDRL_OE_PIN);	// enable  address Bus Low
//
// 		// todo should really sample the high order address bus first
//     	// todo need to support the memory IO for DISK boot block
//     	// load so we can get a loader into 4200H and boot without FreHD
//
//         // wait for ONE of MREQ, RD, WR or IN, OUT to go active(low),
//         // reading LOW address byte at the same time
//         // as address is put on bus before MREQ IORQ
//         do {
//             bus = get_gpio_read_bus();
//         } while ((bus & 0x1F) == 0x1F);
//
// 		// The low order address $xx00 - $xxFF
//         addr = (bus >> (D0_PIN - IN_PIN)) & 0xFF;
//
// 		if (!(bus & 0x01)) // low on iORQ IN
// 		{
// 			if ( (addr >= 0xC0) || (addr <= 0xCF) ) {
//
// 				if (g_byEnableWaitStates) set_gpio(WAIT_PIN);
//
// 				FinishReadOperation(frehd_port_in(addr));
// 			}
// 		}
// 		else if (!(bus & 0x08)) // low on iORQ OUT
// 		{
// 			if ( (addr >= 0xC0) || (addr <= 0xCF) ) {
//
// 				if (g_byEnableWaitStates) set_gpio(WAIT_PIN);
//
// 				frehd_port_out(addr, FinishWriteOperation());
// 			}
// 		}
//         else if (g_byEnableUpperMem)
//         {
//         	// should consider moving this latter
// 			if (g_byEnableWaitStates) set_gpio(WAIT_PIN);
//
// 	        // flip to enable high order address as input
//     	    set_gpio(ADDRL_OE_PIN);
//         	clr_gpio(ADDRH_OE_PIN);
//
// 			NopDelay();	// propagation delay
//
// 			// read high address byte
// 			addr = addr + ((get_gpio_data_byte() & 0xFF) << 8);
//
// 			if (addr >= 0x8000) {
// 				ServiceHighMemoryOperation(addr);
// 			}
// 		}
//
// 		if (g_byEnableWaitStates) clr_gpio(WAIT_PIN); // remove any waits
//
// 		// wait for ALL of MREQ, IN, RD, WR and OUT to go inactive(HIGH)
// 		// this is after the last bus operation
// 		do {
// 			bus = get_gpio_read_bus();
// 		} while ((bus & 0x1F) != 0x1F);
//     }
//
//
// }
