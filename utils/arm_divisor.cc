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

  int goal_freq = 1000000000;

  if (argc >= 2) {
    goal_freq = atoi(argv[1]);
  }
  double divisor = (double)goal_freq / main_crystal / 2;
  int ndiv = (int)divisor & 0x3ff;
  double frac_f = (divisor - ndiv) * (1<<20);
  int frac = frac_f;

  dumpreg(A2W_PLLB_CTRL);
  A2W_PLLB_CTRL = CM_PASSWORD | 0x21000 | ndiv;
  A2W_PLLB_FRAC = CM_PASSWORD | frac;
  dumpreg(A2W_PLLB_CTRL);

  return 0;
}
