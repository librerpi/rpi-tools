#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>
#include <stdlib.h>
#include <stdint.h>
#include <xf86drm.h>
#include <xf86drmMode.h>
#include <cerrno>
#include <string.h>

#include "drm-utils.h"
#include "common.h"

// see also: https://github.com/liujunming/GPU_learning/blob/master/drm/kms-pageflip.c

static void show_frame(int fd, uint32_t crtc_id, uint32_t fb_id) {
  drmModePageFlip(fd, crtc_id, fb_id, DRM_MODE_PAGE_FLIP_EVENT, 0);
}

static void kms() {
  int fd = open("/dev/dri/card1", O_RDWR);
  if (fd < 0) {
    perror("unable to open /dev/dri/card1");
    exit(3);
  }
  uint64_t has_dumb;
  if (drmGetCap(fd, DRM_CAP_DUMB_BUFFER, &has_dumb) < 0 || has_dumb == 0) {
    fatal("drmGetCap DRM_CAP_DUMB_BUFFER failed or doesn't have dumb buffer", errno);
  }
  if (drmSetMaster(fd) < 0) fatal("cant claim to be master of drm device", errno);
  printDrmModes(fd);

  int crtc, connector, encoder;

  crtc = 87; connector = 89; encoder = 88;
  uint32_t pitch0, pitch1, fb_id0, fb_id1;
  uint32_t width, heigth, pitch;

  getDefaultFramebufferSize(fd, crtc, &width, &heigth);
  void *buf0 = createFrameBuffer(fd, width, heigth, &fb_id0, &pitch0);
  void *buf1 = createFrameBuffer(fd, width, heigth, &fb_id1, &pitch1);
  printf("buf0: 0x%x, buf1: 0x%x\n", buf0, buf1);
  for (int y=0; y<heigth; y++) {
    for (int x=0; x<width; x++) {
      uint8_t *pixel = reinterpret_cast<uint8_t*>(buf0 + (y * pitch) + (x * 3));
      pixel[0] = 255;
      pixel[1] = 0;
      pixel[2] = 0;
    }
  }
  for (int y=0; y<heigth; y++) {
    for (int x=0; x<width; x++) {
      uint8_t *pixel = reinterpret_cast<uint8_t*>(buf1 + (y * pitch) + (x * 3));
      pixel[0] = 0;
      pixel[1] = 255;
      pixel[2] = 0;
    }
  }

  showFirstFrame(fd, crtc, connector, fb_id1);

  for (int i=0; i<100; i++) {
    show_frame(fd, crtc, fb_id0);
    struct drm_event_vblank event;
    read(fd, &event, sizeof(event));
    show_frame(fd, crtc, fb_id1);
    read(fd, &event, sizeof(event));
  }

#if 0
  void *buf = setupFrameBuffer(fd, crtc, connector, &width, &heigth, &pitch);
  for (int x=0; x<width; x++) {
    for (int y=0; y<heigth; y++) {
      uint8_t *pixel = reinterpret_cast<uint8_t*>(buf + (y * pitch) + (x * 3));
      pixel[0] = 255;
      pixel[1] = 255;
      pixel[2] = 255;
    }
    usleep(1000);
  }
#endif
}

int main(int argc, char **argv) {
  int opt;
  while((opt = getopt(argc, argv, "k")) != -1) {
    switch (opt) {
    case 'k':
      puts("kms mode");
      kms();
      break;
    }
  }
  return 0;
}
