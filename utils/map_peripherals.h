#pragma once

#define MAJOR_NUM 100
#define IOCTL_MBOX_PROPERTY _IOWR(MAJOR_NUM, 0, char *)
#define DEVICE_FILE_NAME "/dev/vcio"
#define BUS_TO_PHYS(x) ((x)&~0xC0000000)

struct peripherals {
  int fd;
  void *peripherals_start;
  unsigned int physical_start;
};

void open_peripherals(struct peripherals &handle);
int mbox_open();
void mbox_close(int file_desc);
unsigned mem_alloc(int file_desc, unsigned size, unsigned align, unsigned flags);
unsigned mem_free(int file_desc, unsigned handle);
unsigned mem_lock(int file_desc, unsigned handle);
unsigned mem_unlock(int file_desc, unsigned handle);
void *mapmem(unsigned base, unsigned size);
void unmapmem(void *addr, unsigned size);
unsigned execute_code(int file_desc, unsigned code, unsigned r0, unsigned r1, unsigned r2, unsigned r3, unsigned r4, unsigned r5);
