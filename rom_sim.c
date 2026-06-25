#include <arpa/inet.h>
#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <linux/spi/spidev.h>
#include <stdint.h>
#include <stdio.h>
#include <strings.h>
#include <sys/ioctl.h>
#include <sys/stat.h>

int main(int argc, char **argv) {
  int fd = open("/dev/spidev0.0", O_RDWR);
  assert(fd >= 0);
  int mode = 0;
  printf("requesting mode 0x%x\n", mode);
  int ret = ioctl(fd, SPI_IOC_WR_MODE32, &mode); // write mode
  assert(ret == 0);
  ret = ioctl(fd, SPI_IOC_RD_MODE32, &mode); // read mode back
  assert(ret == 0);
  printf("got mode 0x%x\n", mode);

  int bits = 8;
  ret = ioctl(fd, SPI_IOC_WR_BITS_PER_WORD, &bits);
  assert(ret == 0);

  int speed = 500000;
  ret = ioctl(fd, SPI_IOC_WR_MAX_SPEED_HZ, &speed);
  assert(ret == 0);

  uint8_t tx[4];
  uint8_t rx[128<<10];
  bzero(tx, sizeof(tx));
  bzero(rx, sizeof(rx));
  tx[0] = 0x03;
  struct spi_ioc_transfer tr[2] = {
    {
      .tx_buf = tx,
      .len = 4,
    },
    {
      .rx_buf = rx,
      .len = sizeof(rx),
      //.delay_usecs = 0,
      //.word_delay_usecs = 0,
      //.speed_hz = 500000,
      //.bits_per_word = 8,
    }
  };
  ret = ioctl(fd, SPI_IOC_MESSAGE(2), &tr);
  if (ret != ((128<<10) + 4)) {
    perror("failure to SPI_IOC_MESSAGE: ");
    if (errno == EMSGSIZE) {
      puts("you may need to `rmmod spidev ; modprobe -v spidev bufsiz=132096`");
    }
    return 1;
  }
  uint32_t *words = rx;
  printf("reply 0x%x 0x%x\n", ntohl(words[0]), ntohl(words[1]));
  if (ntohl(words[0]) == 0x55aaf00f) {
    printf("correct magic found, stage1 is %d bytes\n", ntohl(words[1]));
    FILE *out = fopen("out.bin", "wb");
    fwrite(&words[2], ntohl(words[1]), 1, out);
    fclose(out);
  }
  return 0;
}
