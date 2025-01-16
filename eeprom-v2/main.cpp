#include <arpa/inet.h>
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <string>
#include <unistd.h>

// hashing code from https://github.com/B-Con/crypto-algorithms
#include "sha256.h"

using namespace std;

#define ROUNDUP(a, b) (((a) + ((b)-1)) & ~((b)-1))

// decompression code based on https://git.venev.name/hristo/rpi-eeprom-compress.git
int uncompress(const uint8_t *data, int size, uint8_t **output) {
  //const uint8_t *start = data;
  uint8_t *uncompressed = (uint8_t*)malloc(1024*1024*16);
  uint8_t *outptr = uncompressed;

  const uint8_t *end = data + size;
  uint8_t out_i = 0;
  char outbuf[256];
  while (data < end) {
    uint8_t c = *data++;
    uint8_t cmd = c;
    //printf("offset 0x%x, cmd 0x%x\n", data-start, cmd);
    for (int i=0; i<8; i++) {
      if (cmd & 1) {
        uint8_t offset = (*data++) + 1;
        uint8_t len = *data++;
        do {
          c = outbuf[(uint8_t)(out_i - offset)];
          outbuf[out_i++] = c;
          *outptr++ = c;
        } while(len--);
      } else {
        if (data >= end) break;
        c = *data++;
        outbuf[out_i++] = c;
        *outptr++ = c;
      }
      cmd >>= 1;
    }
  }
  *output = uncompressed;
  return outptr - uncompressed;
}

void handle_chunk(uint32_t magic, const uint8_t *data, int size, string output_path) {
  switch (magic) {
  case 0x55aaf00f:
  {
    printf("%d byte stage1\n", size);
    string out = output_path + "/bootcode.bin";

    FILE *fp = fopen(out.c_str(), "wb");
    fwrite(data, size, 1, fp);
    fclose(fp);
    break;
  }
  case 0x55aaf11f:
  case 0x55aaf22f:
  {
    const char *name = (const char*)data;
    printf("%d byte plain file, \"%s\"\n", size, name);
    string out = output_path + "/" + name;

    FILE *fp = fopen(out.c_str(), "wb");
    fwrite(data+16, size-16, 1, fp);
    fclose(fp);
    break;
  }
  case 0x55aaf33f:
  {
    uint8_t *uncompressed;
    const char *name = (const char*)data;
    const uint8_t *hash = data + (size - 32);
    printf("%d byte compressed file, \"%s\"\n", size, name);
    int orig_size = uncompress(data + 16, size - 16 - 32, &uncompressed);
    printf("%d\n", orig_size);

    SHA256_CTX ctx;

    sha256_init(&ctx);
    sha256_update(&ctx, uncompressed, orig_size);
    uint8_t actual_hash[32];
    sha256_final(&ctx, actual_hash);

    if (memcmp(hash, actual_hash, 32) == 0) {
      puts("hash match");
      string out = output_path + "/" + name;
      FILE *fp = fopen(out.c_str(), "wb");
      fwrite(uncompressed, orig_size, 1, fp);
      fclose(fp);
      free(uncompressed);
    } else {
      string out = output_path + "/" + name;
      string hashname = out + ".hash";

      FILE *fp = fopen(hashname.c_str(), "wb");
      fwrite(hash, 32, 1, fp);
      fclose(fp);

      string rawname = out + ".raw";

      fp = fopen(rawname.c_str(), "wb");
      fwrite(data + 16, size - 16 - 32, 1, fp);
      fclose(fp);
    }
    break;
  }
  case 0x55aafeef:
    //printf("%d byte alignment padding\n", size);
    break;
  default:
    printf("unhandled magic 0x%x\n", magic);
  }
}

void unpack_eeprom(const uint8_t *bin, int size, const char *output_path) {
  int offset = 0;
  while (offset < size) {
    const uint32_t *ptr = (const uint32_t*)(bin + offset);
    uint32_t magic = ntohl(ptr[0]);
    uint32_t chunk_size = ntohl(ptr[1]);
    if (magic == 0xffffffff) break;
    printf("%x %x, offset 0x%x\n", magic, chunk_size, offset+8);

    const uint8_t *chunk = bin + offset + 8;
    handle_chunk(magic, chunk, chunk_size, output_path);
    offset = ROUNDUP(offset + 8 + chunk_size, 8);
  }
}

int main(int argc, char **argv) {
  int opt;
  const char *input_path = NULL;
  const char *output_path = NULL;
  while ((opt = getopt(argc, argv, "p:o:")) != -1) {
    switch (opt) {
    case 'p':
      input_path = optarg;
      break;
    case 'o':
      output_path = optarg;
      break;
    }
  }
  if (!input_path) {
    fprintf(stderr, "error, no input specified, please use %s -p <input file>\n", argv[0]);
    return 1;
  }

  FILE *eeprom = fopen(input_path, "rb");
  fseek(eeprom, 0L, SEEK_END);
  int size = ftell(eeprom);
  fseek(eeprom, 0, SEEK_SET);

  void *raw_file = malloc(size);
  int res = fread(raw_file, size, 1, eeprom);
  assert(res == 1);
  fclose(eeprom);

  unpack_eeprom((uint8_t*)raw_file, size, output_path);

  free(raw_file);
  return 0;
}
