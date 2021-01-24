#include <stdio.h>
#include "pico/stdlib.h"
#include "hardware/pio.h"
#include "bus6502.pio.h"

#define NUM_PINS 15

static inline void bus6502_program_init(PIO p0, PIO p1, uint pin) {

   // Load the Control program
   uint offset_control = pio_add_program(p0, &bus6502_control_program);

   // Load the PINDIRS program
   uint offset_pindirs = pio_add_program(p1, &bus6502_pindirs_program);

   // Load the PINS0 program (for A2=0)
   uint offset_pins0 = pio_add_program(p1, &bus6502_pins0_program);

   // Load the PINS1 program (for A2=1)
   uint offset_pins1 = pio_add_program(p1, &bus6502_pins1_program);


   // Set the GPIO Function Select to connect the pin to the PIO
   for (uint i = 0; i < NUM_PINS; i++) {
      pio_gpio_init(p0, pin + i);
      pio_gpio_init(p1, pin + i);
   }

   // Set the default pindirs of all state machines to input
   for (uint sm = 0; sm < 4; sm++) {
      pio_sm_set_consecutive_pindirs(p0, sm, pin, NUM_PINS, false);
      pio_sm_set_consecutive_pindirs(p1, sm, pin, NUM_PINS, false);
   }

   // Configure P0 / SM0 (the control state machine)
   pio_sm_config c0 = bus6502_control_program_get_default_config(offset_control);
   sm_config_set_in_pins (&c0, pin       ); // mapping for IN and WAIT
   sm_config_set_jmp_pin (&c0, pin + 8   ); // mapping for JMP (A0)
   sm_config_set_in_shift(&c0, false, true, 32); // shift left, auto push, threshold 32
   pio_sm_init(p0, 0, offset_control, &c0);

   // Configure P1 / SM0 (the PINDIRS state machine controlling the direction of D7:0)
   pio_sm_config c1 = bus6502_pindirs_program_get_default_config(offset_pindirs);
   sm_config_set_in_pins (&c1, pin       ); // mapping for IN and WAIT
   sm_config_set_jmp_pin (&c1, pin + 12  ); // mapping for JMP (RnW)
   sm_config_set_out_pins(&c1, pin,     8); // mapping for OUT (D7:0)
   pio_sm_init(p1, 0, offset_pindirs, &c1);

   // Configure P1 / SM1 (the PIN state machine controlling the data output to D7:0)
   pio_sm_config c2 = bus6502_pins0_program_get_default_config(offset_pins0);
   sm_config_set_in_pins (&c2, pin + 8   ); // mapping for IN and WAIT (A1:0)
   sm_config_set_jmp_pin (&c2, pin + 10  ); // mapping for JMP (A2)
   sm_config_set_out_pins(&c2, pin,     8); // mapping for OUT (D7:0)
   sm_config_set_in_shift(&c2, false, false, 0); // shift left, no auto push
   pio_sm_init(p1, 1, offset_pins0, &c2);

   // Configure PIO2 / SM2 (the PIN state machine controlling the data output to D7:0)
   pio_sm_config c3 = bus6502_pins1_program_get_default_config(offset_pins1);
   sm_config_set_in_pins (&c3, pin + 8   ); // mapping for IN and WAIT (A1:0)
   sm_config_set_jmp_pin (&c3, pin + 10  ); // mapping for JMP (A2)
   sm_config_set_out_pins(&c3, pin,     8); // mapping for OUT (D7:0)
   sm_config_set_in_shift(&c3, false, false, 0); // shift left, no auto push
   pio_sm_init(p1, 2, offset_pins1, &c3);

   // Enable all the state machines
   pio_sm_set_enabled(p0, 0, true);
   for (uint sm = 0; sm < 3; sm++) {
      pio_sm_set_enabled(p1, sm, true);
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

   bus6502_program_init(pio0, pio1, 0);

   // Set X to a value test value (this is used for the read data)

   set_x(pio1, 1, 0x0);
   set_x(pio1, 1, 0x44332211);
   set_x(pio1, 2, 0x88776655);

   while (1) {

      uint32_t value = pio_sm_get_blocking(pio0, 0);
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
