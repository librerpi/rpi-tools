#include <stdint.h>
#include <sys/mman.h>
#include <stdio.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdlib.h>
#include <bcm_host.h>

int main(int argc, char **argv) {
  int fd = open("/dev/mem", O_RDONLY);
  if (fd == -1) {
    perror("unable to open /dev/mem");
    return 2;
  }
  uint64_t physaddr;
  // physaddr = 0x20000000; // bcm2835
  // physaddr = 0xfe000000; // bcm2711
  physaddr = 0x1000000000; // bcm2712
  //physaddr = bcm_host_get_peripheral_address();
  //printf("addr %lx\n", physaddr);
  void *addr = mmap(NULL, 16 * 1024 * 1024, PROT_READ, MAP_SHARED, fd, physaddr);
  if (addr == MAP_FAILED) {
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
  printf("uptimes soc:%lf - linux:%lf == %lf\n", soc_uptime, linux_uptime, soc_uptime - linux_uptime);
  return 0;
}

