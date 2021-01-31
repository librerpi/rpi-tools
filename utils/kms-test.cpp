#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string>
#include <string.h>
#include <sys/mman.h>
#include <unistd.h>
#include <math.h>

#include <xf86drm.h>
#include <xf86drmMode.h>

#include "drm-utils.h"

using namespace std;

void fatal(const char *str) {
  fprintf(stderr, "%s\n", str);
  exit(1);
}

/* emits uart data over DPI
 * config.txt contents:
dpi_output_format=0x17
dpi_group=2
dpi_mode=87
dpi_timings=50 0 0 1 0 2000 0 0 1 0 0 0 0 30 0 5000000 6
dtoverlay=dpi24
enable_dpi_lcd=1
 */
void emitUartData(void *buf, const char *msg, uint32_t width, uint32_t height, uint32_t pitch) {
  int len = strlen(msg);
  for (int y=0; y < height; y++) {
    for (int x=0; x < width; x++) {
      uint8_t *pixel = reinterpret_cast<uint8_t*>(buf + (y * pitch) + (x * 3));
      if ((x/10) < len) {
        if (x % 10 == 0) { // start bit
          pixel[0] = 0;
        } else if (x % 10 == 9) { // stop bit
          pixel[0] = 255;
        } else { // data bits
          int databit = (msg[x/10] >> ((x%10)-1)) & 1;
          pixel[0] = databit ? 0xff : 0x00;
        }
      } else {
        pixel[0] = 0xff;
      }
      pixel[1] = 0; // green
      pixel[2] = 0; // blue
    }
  }
  printf("%d x %d\n", width, height);
}

void emitSineData(void *buf, uint32_t width, uint32_t height, uint32_t pitch) {
  for (int y=0; y < height; y++) {
    for (int x=0; x < width; x++) {
      uint8_t *pixel = reinterpret_cast<uint8_t*>(buf + (y * pitch) + (x * 3));
      double dx = x;
      pixel[0] = (sin((dx / 10) * 3.14) * 0x80) + 0x80;
      pixel[1] = 0;
      pixel[2] = 0;
    }
  }
}

int main(int argc, char **argv) {
  int fd = open("/dev/dri/card1", O_RDWR);
  uint32_t crtc_id, connector_id;
#if 0
  // pi4 DPI
  crtc_id = 52;
  connector_id = 54;
#endif
#if 1
  // pi4 hdmi0
  crtc_id = 81;
  connector_id = 83;
#endif
  if (fd < 0) {
    perror("unable to open /dev/dri/card1");
    exit(3);
  }
  uint64_t has_dumb;
  if (drmGetCap(fd, DRM_CAP_DUMB_BUFFER, &has_dumb) < 0 || has_dumb == 0) {
    fatal("drmGetCap DRM_CAP_DUMB_BUFFER failed or doesn't have dumb buffer");
  }
  drmSetMaster(fd);
  printDrmModes(fd);
  uint32_t width,height,pitch;
  void *buf = setupFrameBuffer(fd, crtc_id, connector_id, &width, &height, &pitch);
  //emitUartData(buf,"Uart test data", width, height, pitch);
  emitSineData(buf, width, height, pitch);
  sleep(100);
  return 0;
}
