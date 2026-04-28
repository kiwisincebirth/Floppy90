//
// Created by Mark Pruden on 29/3/2026.
//

#ifndef FLOPPY90_Z80_BUS_H
#define FLOPPY90_Z80_BUS_H

// INIT
void InitZ80BusGPIO(void);

// The Main Servicing Loop
void __not_in_flash_func(ServiceZ80BusRequest)(void);

// Faster Port Operatons
#define get_gpio_data_byte() (sio_hw->gpio_in >> D0_PIN)
#define get_gpio_read_bus()  (sio_hw->gpio_in >> IN_PIN)
;
#define set_bus_as_output()   sio_hw->gpio_oe_set = 0xFF << D0_PIN
#define set_bus_as_input()    sio_hw->gpio_oe_clr = 0xFF << D0_PIN
;
#define put_byte_on_bus(data) sio_hw->gpio_togl = (sio_hw->gpio_out ^ (data << D0_PIN)) & (0xFF << D0_PIN)

// thsese ones are non-specific, could be improved
#define get_gpio(gpio)       (sio_hw->gpio_in & (1u << gpio))
#define set_gpio(gpio)        sio_hw->gpio_set = 1u << gpio
#define clr_gpio(gpio)        sio_hw->gpio_clr = 1u << gpio

// A (Z80) to B (Pico) direction
#define z80_databus_input() sio_hw->gpio_set = 1u << DIR_PIN

// B (Pico) to A (Z80) direction
#define z80_databus_output() sio_hw->gpio_clr = 1u << DIR_PIN

///////////////////////////////////////////////////////////////////////////////////////////////////

#define NopDelay() __nop(); __nop(); __nop();  __nop(); __nop();
// #define NopDelay() __nop(); __nop(); __nop(); __nop(); __nop(); __nop();

//-----------------------------------------------------------------------------
inline static byte FinishWriteOperation()
{
    set_bus_as_input();
    set_gpio(ADDRL_OE_PIN); 	//ensure address bus is not enabled
    set_gpio(ADDRH_OE_PIN);
    z80_databus_input();        // A (Z80) to B (Pico) direction
    // set_gpio(DIR_PIN);
    clr_gpio(DATAB_OE_PIN);		// enable the data bus

    NopDelay(); // propagation delay ?

    // wait for WR to go active LOW or MREQ to go inactive
    while (get_gpio(WR_PIN) && !get_gpio(MREQ_PIN)) {}

    return get_gpio_data_byte();	// get byte from data bus
}

//-----------------------------------------------------------------------------
inline static void FinishReadOperation(byte data)
{
    // todo consolidate into 4->2 instructions
    set_gpio(ADDRL_OE_PIN); //ensure address bus is not enabled
    set_gpio(ADDRH_OE_PIN);
    z80_databus_output();   // B (Pico) to A (Z80) direction
    // clr_gpio(DIR_PIN);
    set_bus_as_output();    // make data pins (D0-D7) outputs
    clr_gpio(DATAB_OE_PIN); // enable data bus transciever

    put_byte_on_bus(data); // put byte on data bus

    NopDelay(); // propagation delay ?

    // clr_gpio(WAIT_PIN); // remove any waits
}

#endif //FLOPPY90_Z80_BUS_H
