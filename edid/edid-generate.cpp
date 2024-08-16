#include <assert.h>
#include <fcntl.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

void add_detailed(uint8_t *buf, uint32_t pixel_clock,
  uint16_t hactive, uint16_t hfp, uint16_t hsync, uint16_t hbp,
  uint16_t vactive, uint16_t vfp, uint16_t vsync, uint8_t vbp,
  bool interlaced) {
  uint16_t hblank = hfp + hsync + hbp;
  uint16_t vblank = vfp + vsync + vbp;

  uint32_t htotal = hactive + hblank;
  uint32_t vtotal = vactive + vblank;
  uint32_t total = vtotal * htotal;
  printf("hfreq: %f, vfreq: %f\n", (float)pixel_clock / (float)htotal, (float)pixel_clock / (float)total);
  printf("htotal: %d, vtotal: %d\n", htotal, vtotal);
  printf("hblank: %d, hblank: %d\n", hblank, vblank);

  pixel_clock = pixel_clock / (10 * 1000);
  buf[0] = pixel_clock & 0xff;
  buf[1] = pixel_clock >> 8;

  buf[2] = hactive & 0xff;

  buf[3] = hblank & 0xff;;

  buf[4] = (((hactive >> 8) & 0xf)<<4) | ((hblank >> 8) & 0xf);

  buf[5] = vactive & 0xff;

  buf[6] = vblank & 0xff;

  buf[7] = (((vactive >> 8) & 0xf) << 4) | ((vblank >> 8) & 0xf);
  buf[8] = hfp & 0xff;
  buf[9] = hsync & 0xff;
  buf[10] = ((vfp & 0xf) << 4) | (vsync & 0xf);
  buf[11] = (((hfp>>8)&3) << 6) | (((hsync>>8)&3) << 4) | (((vfp>>8)&3)<<2) | ((vsync>>8)&2);
  buf[12] = 40;
  buf[13] = 30;

  buf[17] = (interlaced << 7) | (3 << 3);
}

int main(int argc, char **argv) {
  uint8_t buffer[128];
  memset(buffer, 0, 128);

  buffer[0] = 0;
  buffer[1] = 0xff;
  buffer[2] = 0xff;
  buffer[3] = 0xff;
  buffer[4] = 0xff;
  buffer[5] = 0xff;
  buffer[6] = 0xff;
  buffer[7] = 0;

  buffer[8] = 0;
  buffer[9] = 0;

  buffer[10] = 0;
  buffer[11] = 0;

  buffer[12] = 0;
  buffer[13] = 0;
  buffer[14] = 0;
  buffer[15] = 0;

  buffer[16] = 0xff;

  buffer[17] = (2024 - 1990);

  buffer[18] = 1;
  buffer[19] = 4;

  buffer[20] = (1<<7) | (2 << 4) | 1;

  for (int i=0; i<8; i++) {
    buffer[38+(i*2)] = 1;
    buffer[38+(i*2)+1] = 1;
  }

  add_detailed(buffer + 54 + (18 * 0),
    27 * 1000 * 1000, // pclk
    1440, 40, 118, 118, // hactive/hfp/hsync/hbp
    240, 3, 4, 15, // vactive/vfp/vsync/vbp
    true);

  uint8_t sum = 0;
  for (int i=0; i<128; i++) {
    sum += buffer[i];
  }
  buffer[127] = 256 - sum;

  int fd = open("test.bin", O_WRONLY | O_CREAT, 0755);
  assert(fd >= 0);
  int ret = write(fd, buffer, 128);
  assert(ret == 128);
  close(fd);
}
