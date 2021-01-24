#include <stdio.h>
#include "pico/stdlib.h"
#include "hardware/pio.h"
#include "bus6502.pio.h"

#define NUM_PINS 15

static inline void bus6502_program_init(PIO pio, uint pin) {

   // Load the Control program
   uint offset_control = pio_add_program(pio, &bus6502_control_program);

   // Load the PINS program
   uint offset_pins = pio_add_program(pio, &bus6502_pins_program);

   // Load the PINDIRS program
   uint offset_pindirs = pio_add_program(pio, &bus6502_pindirs_program);

   // Set the GPIO Function Select to connect the pin to the PIO
   for (uint i = 0; i < NUM_PINS; i++) {
      pio_gpio_init(pio, pin + i);
   }

   // Set the default pindirs of all state machines to input
   for (uint sm = 0; sm < NUM_PINS; sm++) {
      pio_sm_set_consecutive_pindirs(pio, sm, pin, NUM_PINS, false);
   }

   // Configure SM0 (the control state machine)
   pio_sm_config c0 = bus6502_control_program_get_default_config(offset_control);
   sm_config_set_in_pins (&c0, pin       ); // mapping for IN and WAIT
   sm_config_set_jmp_pin (&c0, pin + 12  ); // mapping for JMP
   sm_config_set_in_shift(&c0, 0, 0, 0);    // shift left, no auto push
   pio_sm_init(pio, 0, offset_control, &c0);

   // Configure SM1 (the PINDIRS state machine controlling the direction of D3:0)
   pio_sm_config c1 = bus6502_pindirs_program_get_default_config(offset_pindirs);
   sm_config_set_in_pins (&c1, pin       ); // mapping for IN and WAIT
   sm_config_set_jmp_pin (&c1, pin + 12  ); // mapping for JMP
   sm_config_set_set_pins(&c1, pin,     4); // mapping for SET D3:0
   pio_sm_init(pio, 1, offset_pindirs, &c1);

   // Configure SM2 (the PINDIRS state machine controlling the direction of D7:4)
   pio_sm_config c2 = bus6502_pindirs_program_get_default_config(offset_pindirs);
   sm_config_set_in_pins (&c2, pin       ); // mapping for IN and WAIT
   sm_config_set_jmp_pin (&c2, pin + 12  ); // mapping for JMP
   sm_config_set_set_pins(&c2, pin + 4, 4); // mapping for SET D7:4
   pio_sm_init(pio, 2, offset_pindirs, &c2);

   // Configure SM3 (the PIN state machine controlling the data output to D7:0)
   pio_sm_config c3 = bus6502_pins_program_get_default_config(offset_pins);
   sm_config_set_in_pins (&c3, pin + 8   ); // mapping for IN and WAIT
   sm_config_set_jmp_pin (&c3, pin + 12  ); // mapping for JMP (RnW)
   sm_config_set_out_pins(&c3, pin,     8); // mapping for OUT (D7:0)
   sm_config_set_in_shift(&c3, 0, 0, 0);    // shift left, no auto push
   pio_sm_init(pio, 3, offset_pins, &c3);

   // Enable all the state machines
   for (uint sm = 0; sm < NUM_PINS; sm++) {
      pio_sm_set_enabled(pio, sm, true);
   }
}

void set_x(PIO pio, uint sm, uint32_t x) {
   // Write to the TX FIFO
   pio_sm_put(pio, sm, x);
   // execute: pull
   pio_sm_exec(pio, sm, pio_encode_pull(false, false));
   // execute: mov x, osr
   pio_sm_exec(pio, sm, pio_encode_mov(pio_x, pio_osr));
}

int main() {
   stdio_init_all();

   printf("6502 Bus Interface!\n");

   PIO pio = pio0;

   bus6502_program_init(pio, 0);

   // Set X to a value test value (this is used for the read data)

   uint test = 0x44332211;
   set_x(pio, 3, test);

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
         // Toggle X after each write
         //test ^= 0xffffffff;
         //set_x(pio, 0, test);
      }

   }

   return 0;
}
