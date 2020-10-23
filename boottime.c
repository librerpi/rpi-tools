#include <stdint.h>
#include <sys/mman.h>
#include <stdio.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdlib.h>

int main(int argc, char **argv) {
  int fd = open("/dev/mem", O_RDONLY);
  void *addr = mmap(NULL, 16 * 1024 * 1024, PROT_READ, MAP_SHARED, fd, 0xfe000000);
  if (addr == -1) {
    perror("unable to mmap");
    return 1;
  }
  volatile uint32_t *st_clo = (volatile uint32_t*)(addr + 0x3004);
  uint32_t snapshot = *st_clo;
  munmap(addr, 16 * 1024 * 1024);
  close(fd);
  fd = open("/proc/uptime", O_RDONLY);
  char buffer[1024];
  int size = read(fd, buffer, 1024);
  close(fd);
  double soc_uptime = ((float)snapshot / 1000000);
  double linux_uptime = atof(buffer);
  printf("uptimes %lf - %lf == %lf\n", soc_uptime, linux_uptime, soc_uptime - linux_uptime);
  return 0;
}

