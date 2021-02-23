#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <linux/input.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <sys/epoll.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

// see also: https://www.devever.net/~hl/usbnkro

#define MAX_EVENTS 10

static const uint8_t key_mappings[KEY_MAX + 1] = {
  [0 ... KEY_MAX] = 0x0,
  [KEY_A] = 0x04,
  [KEY_B] = 0x05,
  [KEY_C] = 0x06,
  [KEY_D] = 0x07,
  [KEY_E] = 0x08,
  [KEY_F] = 0x09,
  [KEY_G] = 0x0a,
  [KEY_H] = 0x0b,
  [KEY_I] = 0x0c,
  [KEY_J] = 0x0d,
  [KEY_K] = 0x0e,
  [KEY_L] = 0x0f,
  [KEY_M] = 0x10,
  [KEY_N] = 0x11,
  [KEY_O] = 0x12,
  [KEY_P] = 0x13,
  [KEY_Q] = 0x14,
  [KEY_R] = 0x15,
  [KEY_S] = 0x16,
  [KEY_T] = 0x17,
  [KEY_U] = 0x18,
  [KEY_V] = 0x19,
  [KEY_W] = 0x1a,
  [KEY_X] = 0x1b,
  [KEY_Y] = 0x1c,
  [KEY_Z] = 0x1d,

  [KEY_1] = 0x1e,
  [KEY_2] = 0x1f,
  [KEY_3] = 0x20,
  [KEY_4] = 0x21,
  [KEY_5] = 0x22,
  [KEY_6] = 0x23,
  [KEY_7] = 0x24,
  [KEY_8] = 0x25,
  [KEY_9] = 0x26,
  [KEY_0] = 0x27,

  [KEY_ENTER] = 0x28,
  [KEY_ESC]       = 0x29,
  [KEY_BACKSPACE] = 0x2a,
  [KEY_TAB]       = 0x2b,
  [KEY_SPACE]     = 0x2c,
  [KEY_MINUS]     = 0x2d,
  [KEY_EQUAL]     = 0x2e,
  [KEY_LEFTBRACE] = 0x2f,
  [KEY_RIGHTBRACE]= 0x30,
  [KEY_BACKSLASH] = 0x31,
  [KEY_102ND]     = 0x31,
  [KEY_SEMICOLON] = 0x33,
  [KEY_APOSTROPHE]= 0x34,
  [KEY_GRAVE]     = 0x35,
  [KEY_COMMA]     = 0x36,
  [KEY_DOT]       = 0x37,
  [KEY_SLASH]     = 0x38,
  [KEY_CAPSLOCK]  = 0x39,
  [KEY_F1]        = 0x3a,
  [KEY_F2]        = 0x3b,
  [KEY_F3]        = 0x3c,
  [KEY_F4]        = 0x3d,
  [KEY_F5]        = 0x3e,
  [KEY_F6]        = 0x3f,
  [KEY_F7]        = 0x40,
  [KEY_F8]        = 0x41,
  [KEY_F9]        = 0x42,
  [KEY_F10]       = 0x43,
  [KEY_F11]       = 0x44,
  [KEY_F12]       = 0x45,
  [KEY_INSERT]    = 0x49,
  [KEY_HOME]      = 0x4a,
  [KEY_PAGEUP]    = 0x4b,
  [KEY_DELETE]    = 0x4c,
  [KEY_END]       = 0x4d,
  [KEY_PAGEDOWN]  = 0x4e,
  [KEY_RIGHT]     = 0x4f,
  [KEY_LEFT]      = 0x50,
  [KEY_DOWN]      = 0x51,
  [KEY_UP]        = 0x52,
  [KEY_NUMLOCK]   = 0x53,
  [KEY_SYSRQ]     = 0,
};

int main(int argc, char **argv) {
  struct epoll_event events[MAX_EVENTS];
  int fd = open("/dev/input/event0", O_RDWR);
  assert(fd >= 0);
  int ret = ioctl(fd, EVIOCGRAB, (void*)1);
  assert(ret == 0);

  int epollfd = epoll_create1(0);
  assert(epollfd >= 0);

  struct epoll_event ev;
  ev.events = EPOLLIN;
  ev.data.fd = fd;
  if (epoll_ctl(epollfd, EPOLL_CTL_ADD, fd, &ev)) {
    perror("unable to epoll add");
    return 1;
  }

  int hidout = open("/dev/hidg0", O_RDWR);
  assert(hidout >= 0);
  uint8_t report[8];
  printf("hidout: %d\n", hidout);

  uint8_t modifiers = 0;

  while (1) {
    int nfds = epoll_wait(epollfd, events, MAX_EVENTS, -1);
    if (nfds < 0) {
      printf("%d ", nfds);
      perror("epoll_wait failed");
      return 2;
    }
    int keys = 0;
    memset(report, 0, 8);
    for (int i=0; i < nfds; i++) {
      struct input_event ev[64];
      int rd = read(events[i].data.fd, ev, sizeof(ev));
      assert(rd > ((signed int)sizeof(struct input_event)));
      for (int j=0; j < rd / ((signed int)sizeof(struct input_event)); j++) {
        unsigned int type, code;
        type = ev[j].type;
        code = ev[j].code;
        //printf("Event: time %ld.%06ld, ", ev[j].time.tv_sec, ev[j].time.tv_usec);
        if (type == EV_KEY) {
          if (key_mappings[code] == 0) {
            printf("code %d %d report:%x\n", code, ev[j].value, key_mappings[code]);
          }
          uint8_t mod = 0;
          switch (code) {
          case KEY_LEFTCTRL:
            mod = 0x01;
            break;
          case KEY_RIGHTCTRL:
            mod = 0x10;
            break;
          case KEY_LEFTSHIFT:
            mod = 0x02;
            break;
          case KEY_RIGHTSHIFT:
            mod = 0x20;
            break;
          case KEY_LEFTALT:
            mod = 0x04;
            break;
          case KEY_RIGHTALT:
            mod = 0x40;
            break;
          case KEY_LEFTMETA:
            mod = 0x08;
            break;
          }
          if (ev[j].value) {
            modifiers |= mod;
          } else {
            modifiers &= ~mod;
          }
          report[0] = modifiers;
          if (ev[j].value) {
            if (keys < 6) {
              report[2+keys] = key_mappings[code];
              keys++;
            }
          }
        }
      }
    }
    int wr = write(hidout, report, 8);
    if (wr != 8) {
      perror("problem writing to hidout");
    }
    //puts("");
  }
  return 0;
}
