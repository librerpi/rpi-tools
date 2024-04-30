#include <assert.h>
#include <stdlib.h>
#include <stdio.h>
#include <elf.h>
#include <fcntl.h>
#include <stdbool.h>
#include <strings.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#define ROUNDUP(a, b) (((a) + ((b)-1)) & ~((b)-1))

int main(int argc, char **argv) {
  int opt;
  char *in = NULL;
  char *out = NULL;
  bool rom = true;
  while ((opt = getopt(argc, argv, "i:o:")) != -1) {
    switch (opt) {
    case 'i':
      in = optarg;
      break;
    case 'o':
      out = optarg;
      break;
    }
  }
  assert(in);
  assert(out);
  int rom_in = open(in, O_RDONLY);
  int elf_out = open(out, O_RDWR | O_CREAT, 0644);

  assert(rom_in > 0);
  assert(elf_out > 0);

  uint64_t bin_size = lseek(rom_in, 0, SEEK_END);
  uint8_t *wholebin = malloc(bin_size);
  //printf("%p\n", wholebin);
  pread(rom_in, wholebin, bin_size, 0);

  uint32_t dests[4];
  uint32_t srcs[4];
  int32_t sizes[4];
  int next_slot = 0;

  uint32_t magic = 0x02494e49;
  uint32_t *casted = (uint32_t*)wholebin;
  for (int i=0; i<(bin_size/4); i++) {
    if (casted[i] == magic) {
      printf("found at 0x%x\n", i*4);
      uint32_t *load = &casted[i+1];
      while (load[0] && load[1]) {
        assert(next_slot < 4);
        uint32_t addr = load[0];
        int32_t size = load[1];
        printf("0x%x %d\n", addr, size);
        dests[next_slot] = addr;
        if (size > 0) {
          srcs[next_slot] = ((uint8_t*)&load[2]) - wholebin;
        }
        sizes[next_slot] = size;
        if (size > 0) {
          load = load + ((8 + (ROUNDUP(size,4)))/4);
        } else {
          load = load + 2;
        }
        //printf("%p\n", load);
        next_slot++;
      }
    }
  }

  Elf32_Ehdr elf_header;
  Elf32_Phdr phdr[1 + next_slot];

  bzero(&elf_header, sizeof(elf_header));
  bzero(&phdr, sizeof(phdr));

  elf_header.e_ident[EI_MAG0] = ELFMAG0;
  elf_header.e_ident[EI_MAG1] = ELFMAG1;
  elf_header.e_ident[EI_MAG2] = ELFMAG2;
  elf_header.e_ident[EI_MAG3] = ELFMAG3;
  elf_header.e_ident[EI_CLASS] = ELFCLASS32;
  elf_header.e_ident[EI_DATA] = ELFDATA2LSB;
  elf_header.e_ident[EI_VERSION] = EV_CURRENT;
  elf_header.e_ident[EI_OSABI] = ELFOSABI_SYSV;
  //elf_header.e_ident[EI_ABIVERSION] = ;
  elf_header.e_ident[EI_NIDENT] = sizeof(elf_header.e_ident);
  elf_header.e_type = ET_EXEC;
  elf_header.e_machine = 137; // VC4
  elf_header.e_version = EV_CURRENT;
  if (rom) elf_header.e_entry = 0x60000000;
  elf_header.e_phoff = 64; // offset to program header
  elf_header.e_phentsize = sizeof(phdr[0]);
  elf_header.e_phnum = sizeof(phdr) / sizeof(phdr[0]);

  phdr[0].p_type = PT_LOAD;
  phdr[0].p_offset = 0x1000;
  phdr[0].p_vaddr = 0x60000000;
  phdr[0].p_paddr = phdr[0].p_vaddr;
  phdr[0].p_filesz = bin_size;
  phdr[0].p_memsz = bin_size;
  phdr[0].p_flags = PF_X | PF_R;
  phdr[0].p_align = 0;

  for (int i=0; i<next_slot; i++) {
    phdr[1+i].p_type = PT_LOAD;
    if (sizes[i] > 0) {
      phdr[1+i].p_offset = 0x1000 + srcs[i];
      phdr[1+i].p_filesz = sizes[i];
    }
    phdr[1+i].p_vaddr = phdr[1+i].p_paddr = dests[i];
    phdr[1+i].p_memsz = abs(sizes[i]);
  }

  pwrite(elf_out, &elf_header, sizeof(elf_header), 0);
  pwrite(elf_out, &phdr, sizeof(phdr), 64);

  pwrite(elf_out, wholebin, bin_size, 0x1000);


  return 0;
}
