#include <xf86drm.h>
#include <xf86drmMode.h>
#include <stdio.h>
#include <string>
#include <string.h>
#include <sys/mman.h>

#include "drm-utils.h"
#include "common.h"

using namespace std;

void getDefaultFramebufferSize(int fd, uint32_t crtc_id, uint32_t *width, uint32_t* heigth) {
  drmModeCrtcPtr crtc_info = drmModeGetCrtc(fd, crtc_id);
  *width = crtc_info->mode.hdisplay;
  *heigth = crtc_info->mode.vdisplay;

  drmModeFreeCrtc(crtc_info);
}

void showFirstFrame(int fd, uint32_t crtc_id, uint32_t conn_id, uint32_t fb_id) {
  drmModeCrtcPtr crtc_info = drmModeGetCrtc(fd, crtc_id);
  printf("showFirstFrame fb_id==%d\n", fb_id);
  if (drmModeSetCrtc(fd, crtc_id, fb_id, 0, 0, &conn_id, 1, &crtc_info->mode)) {
    fatal("drmModeSetCrtc() failed", errno);
  }
  drmModeFreeCrtc(crtc_info);
}

void *createFrameBuffer(int fd, uint32_t width, uint32_t heigth, uint32_t *fb_id, uint32_t *pitch) {
  struct drm_mode_create_dumb creq;
  printf("creating fb...\n");

  memset(&creq, 0, sizeof(struct drm_mode_create_dumb));
  creq.width = width;
  creq.height = heigth;
  creq.bpp = 24;
  printf("%d x %d\n", creq.width, creq.height);

  // create framebuffer
  if (drmIoctl(fd, DRM_IOCTL_MODE_CREATE_DUMB, &creq) < 0) {
    fatal("drmIoctl DRM_IOCTL_MODE_CREATE_DUMB failed", errno);
  }

  printf("drmModeAddFB(%d, %d, %d, 24, 24, %d, %d, fb_id)\n", fd, width, heigth, creq.pitch, creq.handle);
  // add framebuffer to something
  if (drmModeAddFB(fd, width, heigth, 24, 24, creq.pitch, creq.handle, fb_id)) {
    fatal("drmModeAddFB failed", errno);
  }
  printf("fb_id: %d\n", *fb_id);
  printf("pitch: %d\n", creq.pitch);
  *pitch = creq.pitch;

  struct drm_mode_map_dumb mreq;
  memset(&mreq, 0, sizeof(struct drm_mode_map_dumb));
  mreq.handle = creq.handle;

  if (drmIoctl(fd, DRM_IOCTL_MODE_MAP_DUMB, &mreq)) {
    fatal("drmIoctl DRM_IOCTL_MODE_MAP_DUMB failed", errno);
  }

  void *buf = (void *) mmap(0, creq.size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, mreq.offset);
  if (buf == MAP_FAILED) {
    perror("cant mmap fb");
    exit(2);
  }

  return buf;
}

void *setupFrameBuffer(int fd, int crtc_id, uint32_t conn_id, uint32_t *width, uint32_t *heigth, uint32_t *pitch) {
  struct drm_mode_create_dumb creq;
  struct drm_mode_map_dumb mreq;
  uint32_t fb_id;
  void *buf;
  drmModeCrtcPtr crtc_info = drmModeGetCrtc(fd, crtc_id);

  memset(&creq, 0, sizeof(struct drm_mode_create_dumb));
  creq.width = crtc_info->mode.hdisplay;
  creq.height = crtc_info->mode.vdisplay;
  creq.bpp = 24;
  printf("%d x %d\n", creq.width, creq.height);

  if (drmIoctl(fd, DRM_IOCTL_MODE_CREATE_DUMB, &creq) < 0) {
    fatal("drmIoctl DRM_IOCTL_MODE_CREATE_DUMB failed", errno);
  }

  printf("drmModeAddFB(%d, %d, %d, 24, 24, %d, %d, fb_id)\n", fd, crtc_info->mode.hdisplay, crtc_info->mode.vdisplay, creq.pitch, creq.handle);
  if (drmModeAddFB(fd, crtc_info->mode.hdisplay, crtc_info->mode.vdisplay, 24, 24, creq.pitch, creq.handle, &fb_id)) {
    fatal("drmModeAddFB failed", errno);
  }
  printf("fb_id: %d\n", fb_id);
  printf("pitch: %d\n", creq.pitch);

  memset(&mreq, 0, sizeof(struct drm_mode_map_dumb));
  mreq.handle = creq.handle;

  if (drmIoctl(fd, DRM_IOCTL_MODE_MAP_DUMB, &mreq)) {
    fatal("drmIoctl DRM_IOCTL_MODE_MAP_DUMB failed", errno);
  }

  buf = (void *) mmap(0, creq.size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, mreq.offset);
  if (buf == MAP_FAILED) {
    perror("cant mmap fb");
    exit(2);
  }

  if (drmModeSetCrtc(fd, crtc_id, fb_id, 0, 0, &conn_id, 1, &crtc_info->mode)) {
    fatal("drmModeSetCrtc() failed", errno);
  }
  drmModeFreeCrtc(crtc_info);
  *width = creq.width;
  *heigth = creq.height;
  *pitch = creq.pitch;
  return buf;
}

string connectorTypeToStr(uint32_t type) {
  switch (type) {
  case DRM_MODE_CONNECTOR_HDMIA: // 11
    return "HDMIA";
  case DRM_MODE_CONNECTOR_DSI: // 16
    return "DSI";
  }
  return "unknown";
}

void printDrmModes(int fd) {
  drmVersionPtr version = drmGetVersion(fd);
  printf("version %d.%d.%d\nname: %s\ndate: %s\ndescription: %s\n", version->version_major, version->version_minor, version->version_patchlevel, version->name, version->date, version->desc);
  drmFreeVersion(version);
  drmModeRes * modes = drmModeGetResources(fd);
  for (int i=0; i < modes->count_fbs; i++) {
    printf("FB#%d: %x\n", i, modes->fbs[i]);
  }
  for (int i=0; i < modes->count_crtcs; i++) {
    printf("CRTC#%d: %d\n", i, modes->crtcs[i]);
    drmModeCrtcPtr crtc = drmModeGetCrtc(fd, modes->crtcs[i]);
    printf("  buffer_id: %d\n", crtc->buffer_id);
    printf("  position: %dx%d\n", crtc->x, crtc->y);
    printf("  size: %dx%d\n", crtc->width, crtc->height);
    printf("  mode_valid: %d\n", crtc->mode_valid);
    printf("  gamma_size: %d\n", crtc->gamma_size);
    printf("  Mode\n    clock: %d\n", crtc->mode.clock);
    drmModeModeInfo &mode = crtc->mode;
    printf("    h timings: %d %d %d %d %d\n", mode.hdisplay, mode.hsync_start, mode.hsync_end, mode.htotal, mode.hskew);
    printf("    v timings: %d %d %d %d %d\n", mode.vdisplay, mode.vsync_start, mode.vsync_end, mode.vtotal, mode.vscan);
    printf("    vrefresh: %d\n", mode.vrefresh);
    printf("    flags: 0x%x\n", mode.flags);
    printf("    type: %d\n", mode.type);
    printf("    name: %s\n", mode.name);
    drmModeFreeCrtc(crtc);
  }
  for (int i=0; i < modes->count_connectors; i++) {
    printf("Connector#%d: %d\n", i, modes->connectors[i]);
    drmModeConnectorPtr connector = drmModeGetConnector(fd, modes->connectors[i]);
    if (connector->connection == DRM_MODE_CONNECTED) puts("  connected!");
    string typeStr = connectorTypeToStr(connector->connector_type);
    printf("  ID: %d\n  Encoder: %d\n  Type: %d %s\n  type_id: %d\n  physical size: %dx%d\n", connector->connector_id, connector->encoder_id, connector->connector_type, typeStr.c_str(), connector->connector_type_id, connector->mmWidth, connector->mmHeight);
    for (int j=0; j < connector->count_encoders; j++) {
      printf("  Encoder#%d:\n", j);
      drmModeEncoderPtr enc = drmModeGetEncoder(fd, connector->encoders[j]);
      printf("    ID: %d\n    Type: %d\n    CRTCs: 0x%x\n    Clones: 0x%x\n", enc->encoder_id, enc->encoder_type, enc->possible_crtcs, enc->possible_clones);
      drmModeFreeEncoder(enc);
    }
    printf("  Modes: %d\n", connector->count_modes);
    for (int j=0; j < connector->count_modes; j++) {
      printf("  Mode#%d:\n", j);
      if (j > 1) break;
      drmModeModeInfo &mode = connector->modes[j];
      printf("    clock: %d\n", mode.clock);
      printf("    h timings: %d %d %d %d %d\n", mode.hdisplay, mode.hsync_start, mode.hsync_end, mode.htotal, mode.hskew);
      printf("    v timings: %d %d %d %d %d\n", mode.vdisplay, mode.vsync_start, mode.vsync_end, mode.vtotal, mode.vscan);
      printf("    vrefresh: %d\n", mode.vrefresh);
      printf("    flags: 0x%x\n", mode.flags);
      printf("    type: %d\n", mode.type);
      printf("    name: %s\n", mode.name);
    }
    drmModeFreeConnector(connector);
  }
  for (int i=0; i < modes->count_encoders; i++) {
    printf("Encoder#%d: %d\n", i, modes->encoders[i]);
  }
  printf("min size: %dx%d\n", modes->min_width, modes->min_height);
  printf("max size: %dx%d\n", modes->max_width, modes->max_height);
  drmModeFreeResources(modes);
}
