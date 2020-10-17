#include <fcntl.h>
#include <hexdump.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include <vc_mem.h>

int main(int argc, char **argv) {
  int opt;
  char *device = NULL;
  uint32_t addr = 0;
  uint32_t length = 32;
  bool vcmem = false;
  while ((opt = getopt(argc, argv, "mvgd:a:l:")) != -1) {
    switch (opt) {
    case 'm':
      device = "/dev/mem";
      break;
    case 'v':
      device = "/dev/vc-mem";
      vcmem = true;
      break;
    case 'g':
      device = "/dev/gpiomem";
      break;
    case 'd':
      device = optarg;
      break;
    case 'a':
      addr = strtoll(optarg, NULL, 0);
      break;
    case 'l':
      length = strtol(optarg, NULL, 0);
      break;
    }
  }
  if (device == NULL) {
    puts("error, you must specify a device\n/dev/gpiomem is -g\n/dev/mem is -m\n/dev/vc-mem is -v\n");
    return 4;
  }

  int fd = open(device, O_RDONLY);
  if (fd < 0) {
    printf("unable to open %s, ", device);
    perror("reason");
    return 2;
  }
  if (vcmem) {
    uint32_t extra_offset = 0;
    int ret = ioctl(fd, VC_MEM_IOC_MEM_BASE, &extra_offset);
    if (ret == -1) {
      perror("ioctl error");
      return 5;
    }
    addr += extra_offset;
  }
  uint32_t read_offset = addr & 0xfff;
  addr = (addr >> 12) << 12;
  volatile void *rawaddr = (uint32_t*)mmap(NULL, length + read_offset, PROT_READ, MAP_SHARED, fd, addr);
  if (rawaddr == MAP_FAILED) {
    perror("unable to mmap");
    return 3;
  }
  printf("starting at 0x%x (%dMB)\n", addr+read_offset, (addr+read_offset)/1024/1024);
  hexdump_ram(rawaddr+read_offset, addr+read_offset, length);
  return 0;
}
