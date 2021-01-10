#include <stdio.h>
#include <bcm_host.h>
#include <hardware.h>
#include "map_peripherals.h"

int main_crystal;

#define dumpreg(reg) { t = reg; printf(#reg":\t 0x%x\n", t); }

int main(int argc, char **argv) {
  uint32_t t;
  struct peripherals handle;
  open_peripherals(handle);
  void *mmiobase = handle.peripherals_start;
  if (bcm_host_is_model_pi4()) main_crystal = 54000000;
  else main_crystal = 19200000;

  dumpreg(A2W_PLLB_CTRL);
  A2W_PLLB_CTRL = CM_PASSWORD | 0x21010;
  dumpreg(A2W_PLLB_CTRL);

  return 0;
}
