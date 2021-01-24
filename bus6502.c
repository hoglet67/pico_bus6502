#include <stdio.h>
#include "pico/stdlib.h"
#include "hardware/pio.h"
#include "bus6502.pio.h"

#define NUM_PINS 15

static inline void bus6502_program_init(PIO pio, uint pin) {

   // Load the R/W program
   uint offset_rw = pio_add_program(pio, &bus6502_rw_program);

   // Load the PINDIRS program
   uint offset_pindirs = pio_add_program(pio, &bus6502_pindirs_program);

   // Set the GPIO Function Select to connect the pin to the PIO
   for (uint i = 0; i < NUM_PINS; i++) {
      pio_gpio_init(pio, pin + i);
   }

   // Set the default pindirs of all state machines to input
   pio_sm_set_consecutive_pindirs(pio, 0, pin, NUM_PINS, false);
   pio_sm_set_consecutive_pindirs(pio, 1, pin, NUM_PINS, false);
   pio_sm_set_consecutive_pindirs(pio, 2, pin, NUM_PINS, false);

   // Configure SM0 (the R/W state machine)
   pio_sm_config c0 = bus6502_rw_program_get_default_config(offset_rw);
   sm_config_set_in_pins (&c0, pin       ); // mapping for IN and WAIT
   sm_config_set_out_pins(&c0, pin,     8); // mapping for OUT (D7:0)
   sm_config_set_jmp_pin (&c0, pin + 12  ); // mapping for JMP
   sm_config_set_in_shift(&c0, 0, 0, 0);    // shift left, no auto push
   pio_sm_init(pio, 0, offset_rw, &c0);

   // Configure SM1 (the PINDIRS state machine controlling the direction of D3:0)
   pio_sm_config c1 = bus6502_pindirs_program_get_default_config(offset_pindirs);
   sm_config_set_in_pins (&c1, pin       ); // mapping for IN and WAIT
   sm_config_set_set_pins(&c1, pin,     4); // mapping for SET D3:0
   sm_config_set_jmp_pin (&c1, pin + 12  ); // mapping for JMP
   pio_sm_init(pio, 1, offset_pindirs, &c1);

   // Configure SM2 (the PINDIRS state machine controlling the direction of D7:4)
   pio_sm_config c2 = bus6502_pindirs_program_get_default_config(offset_pindirs);
   sm_config_set_in_pins (&c2, pin       ); // mapping for IN and WAIT
   sm_config_set_set_pins(&c2, pin + 4, 4); // mapping for SET D7:4
   sm_config_set_jmp_pin (&c2, pin + 12  ); // mapping for JMP
   pio_sm_init(pio, 2, offset_pindirs, &c2);

   // Enable all the state machines
   pio_sm_set_enabled(pio, 0, true);
   pio_sm_set_enabled(pio, 1, true);
   pio_sm_set_enabled(pio, 2, true);

}

int main() {
   stdio_init_all();

   printf("6502 Bus Interface!\n");

   PIO pio = pio0;

   bus6502_program_init(pio, 0);

   // Set X to a value test value
   pio_sm_exec(pio, 0, 0xE000 + 0x20 + 0x15);

   while (1) {
      uint32_t value = pio_sm_get_blocking(pio, 0);
      printf("raw=%08x\n", value);

      uint data =  value        & 0xff;
      uint addr = (value >> 24) & 0x0f;
      uint rnw  = (value >> 28) & 0x01;
      if (rnw) {
         printf("read:  addr=%x data=%02x\n", addr, data);
      } else {
         printf("write: addr=%x data=%02x\n", addr, data);
      }
   }

   return 0;
}
