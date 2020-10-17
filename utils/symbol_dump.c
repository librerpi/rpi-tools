#include <fcntl.h>
#include <hexdump.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include <vc_mem.h>

#define VPU_TO_VIRT(addr) ((addr - firmware_base) + rawaddr)
#define REG32(addr) ((volatile uint32_t *)(VPU_TO_VIRT(addr)))

int main(int argc, char **argv) {
  int fd = open("/dev/vc-mem", O_RDONLY);
  if (fd < 0) {
    perror("unable to open /dev/vc-mem");
    return 2;
  }

  uint32_t firmware_base;
  int ret = ioctl(fd, VC_MEM_IOC_MEM_BASE, &firmware_base);
  if (ret == -1) {
    perror("ioctl error");
    return 5;
  }

  uint32_t vcmem_size;
  ret = ioctl(fd, VC_MEM_IOC_MEM_SIZE, &vcmem_size);
  if (ret == -1) {
    perror("ioctl error");
    return 5;
  }

  printf("firmware starts at 0x%x and ends at 0x%x\n", firmware_base, vcmem_size);
  volatile void *rawaddr = (uint32_t*)mmap(NULL, vcmem_size - firmware_base, PROT_READ, MAP_SHARED, fd, firmware_base);

  hexdump_ram(rawaddr + 0x2800, firmware_base + 0x2800, 0x30);

  uint32_t symbol_table = *REG32(firmware_base + 0x2800);
  printf("symbol table is at 0x%x\n", symbol_table);
  uint32_t log_start, log_end;
  while (true) {
    uint32_t name_addr = *REG32(symbol_table);
    char *name = VPU_TO_VIRT(name_addr);
    uint32_t symbol_addr = *REG32(symbol_table+4);
    uint32_t flags = *REG32(symbol_table+8);
    if (name_addr == 0) break;
    bool string_based = false;
    switch (flags) {
      case 0x6:
      case 0x9:
      case 0xa:
      case 0xc:
      case 0x12:
      case 0x31:
        string_based = true;
        break;
    }
    if (strcmp(name, "vcos_build_user") == 0) string_based = true;
    if (strcmp(name, "__LOG_START") == 0) log_start = *REG32(symbol_addr & 0x3fffffff) & 0x3fffffff;
    if (strcmp(name, "__LOG_END") == 0) log_end = *REG32(symbol_addr & 0x3fffffff) & 0x3fffffff;
    if (string_based) {
      printf("%s == %s\n", name, VPU_TO_VIRT(symbol_addr));
    } else {
      printf("0x%x 0x%03x %s\n", symbol_addr, flags, name);
    }
    symbol_table += 12;
  }
  printf("logs span 0x%x to 0x%x\n", log_start, log_end);
  hexdump_ram(VPU_TO_VIRT(log_start), log_start, log_end - log_start);
  return 0;
}
