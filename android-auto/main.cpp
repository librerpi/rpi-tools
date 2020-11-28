// heavily based on https://blog.soutade.fr/post/2016/07/create-your-own-usb-gadget-with-gadgetfs.html
// and also https://github.com/viktorgino/libheadunit

#include <assert.h>
#include <sys/ioctl.h>
#include <sys/select.h>
#include <sys/stat.h>
#include <sys/types.h>

#include <linux/types.h>
#include <linux/usb/ch9.h>
#include <linux/usb/gadgetfs.h>

#include <fcntl.h>
#include <stdio.h>
#include <unistd.h>
#include <stdint.h>
#include <string.h>
#include <pthread.h>

#include <errno.h>

#include "usbstring.h"
#include "aap.h"

#define FETCH(_var_)                            \
    memcpy(cp, &_var_, _var_.bLength);          \
    cp += _var_.bLength;

#define CONFIG_VALUE 2

// Specific to controller
#define USB_DEV "/dev/gadget/fe980000.usb"
#define USB_EPIN "/dev/gadget/ep1in"
#define USB_EPOUT "/dev/gadget/ep4out"

enum {
    STRINGID_MANUFACTURER = 1,
    STRINGID_PRODUCT,
    STRINGID_SERIAL,
    STRINGID_CONFIG_HS,
    STRINGID_CONFIG_LS,
    STRINGID_INTERFACE,
    STRINGID_MAX
};

struct io_thread_args {
    unsigned stop;
    int fd_in, fd_out;
};

static struct io_thread_args thread_args;

static struct usb_string stringtab [] = {
    { STRINGID_MANUFACTURER, "https://github.com/cleverca22", },
    { STRINGID_PRODUCT,      "android-auto gadget", },
    { STRINGID_SERIAL,       "0001", },
    { STRINGID_CONFIG_HS,    "High speed configuration", },
    { STRINGID_CONFIG_LS,    "Low speed configuration", },
    { STRINGID_INTERFACE,    "Android Accessory Interface", },
    { STRINGID_MAX, NULL},
};

static struct usb_gadget_strings strings = {
    .language = 0x0409, /* en-us */
    .strings = stringtab,
};


static void* io_thread(void* arg);

class UsbGadget {
public:
  UsbGadget();
  ~UsbGadget();
  int openGadget();
  void populateDeviceDescriptor(struct usb_device_descriptor &dev);
  void populateEndpointDescriptors();
  void populateInterfaceDescriptor(struct usb_interface_descriptor &if_descriptor);
  int init_ep(int* fd_in, int* fd_out);
  void handle_setup_request(int fd, struct usb_ctrlrequest* setup);
  void handle_ep0();
private:
  struct usb_endpoint_descriptor ep_descriptor_in;
  struct usb_endpoint_descriptor ep_descriptor_out;
  int fd;
};

UsbGadget::UsbGadget() : fd(-1) {
}
UsbGadget::~UsbGadget() {
  if (fd != -1) close(fd);
}

void UsbGadget::populateDeviceDescriptor(struct usb_device_descriptor &device_descriptor) {
  device_descriptor.bLength = USB_DT_DEVICE_SIZE;
  device_descriptor.bDescriptorType = USB_DT_DEVICE;
  device_descriptor.bDeviceClass = 0;
  device_descriptor.bDeviceSubClass = 0;
  device_descriptor.bDeviceProtocol = 0;
  //device_descriptor.bMaxPacketSize0 = 255; Set by driver
  device_descriptor.idVendor = 0x18D1;
  device_descriptor.idProduct = 0x2D00;
  device_descriptor.bcdDevice = 0xffff; // Version
  // Strings
  device_descriptor.iManufacturer = STRINGID_MANUFACTURER;
  device_descriptor.iProduct = STRINGID_PRODUCT;
  device_descriptor.iSerialNumber = STRINGID_SERIAL;
  device_descriptor.bNumConfigurations = 1; // Only one configuration
}

void UsbGadget::populateEndpointDescriptors() {
  ep_descriptor_in.bLength = USB_DT_ENDPOINT_SIZE;
  ep_descriptor_in.bDescriptorType = USB_DT_ENDPOINT;
  ep_descriptor_in.bEndpointAddress = USB_DIR_IN | 1;
  ep_descriptor_in.bmAttributes = USB_ENDPOINT_XFER_BULK;
  ep_descriptor_in.wMaxPacketSize = 512; // HS size

  ep_descriptor_out.bLength = USB_DT_ENDPOINT_SIZE;
  ep_descriptor_out.bDescriptorType = USB_DT_ENDPOINT;
  ep_descriptor_out.bEndpointAddress = USB_DIR_OUT | 4;
  ep_descriptor_out.bmAttributes = USB_ENDPOINT_XFER_BULK;
  ep_descriptor_out.wMaxPacketSize = 512; // HS size
}

void UsbGadget::populateInterfaceDescriptor(struct usb_interface_descriptor &if_descriptor) {
  if_descriptor.bLength = sizeof(if_descriptor);
  if_descriptor.bDescriptorType = USB_DT_INTERFACE;
  if_descriptor.bInterfaceNumber = 0;
  if_descriptor.bAlternateSetting = 0;
  if_descriptor.bNumEndpoints = 2;
  if_descriptor.bInterfaceClass = 255;
  if_descriptor.bInterfaceSubClass = 255;
  if_descriptor.bInterfaceProtocol = 0;
  if_descriptor.iInterface = STRINGID_INTERFACE;
}

int UsbGadget::openGadget() {
  uint8_t init_config[2048];
  uint8_t* cp;
  struct usb_device_descriptor device_descriptor;
  struct usb_interface_descriptor if_descriptor;
  struct usb_config_descriptor config;
  struct usb_config_descriptor config_hs;
  uint32_t send_size;
  uint32_t ret;

  fd = open(USB_DEV, O_RDWR|O_SYNC);

  if (fd <= 0) {
    printf("Unable to open %s (%m)\n", USB_DEV);
    return 1;
  }

  *(uint32_t*)init_config = 0;
  cp = &init_config[4];
  populateDeviceDescriptor(device_descriptor);
  populateEndpointDescriptors();
  populateInterfaceDescriptor(if_descriptor);

  config_hs.bLength = sizeof(config_hs);
  config_hs.bDescriptorType = USB_DT_CONFIG;
  config_hs.wTotalLength = config_hs.bLength +
      if_descriptor.bLength + ep_descriptor_in.bLength + ep_descriptor_out.bLength;
  config_hs.bNumInterfaces = 1;
  config_hs.bConfigurationValue = CONFIG_VALUE;
  config_hs.iConfiguration = STRINGID_CONFIG_HS;
  config_hs.bmAttributes = USB_CONFIG_ATT_ONE | USB_CONFIG_ATT_SELFPOWER;
  config_hs.bMaxPower = 1;

  config.bLength = sizeof(config);
  config.bDescriptorType = USB_DT_CONFIG;
  config.wTotalLength = config.bLength +
      if_descriptor.bLength + ep_descriptor_in.bLength + ep_descriptor_out.bLength;
  config.bNumInterfaces = 1;
  config.bConfigurationValue = CONFIG_VALUE;
  config.iConfiguration = STRINGID_CONFIG_LS;
  config.bmAttributes = USB_CONFIG_ATT_ONE | USB_CONFIG_ATT_SELFPOWER;
  config.bMaxPower = 1;

  FETCH(config);
  FETCH(if_descriptor);
  FETCH(ep_descriptor_in);
  FETCH(ep_descriptor_out);

  FETCH(config_hs);
  FETCH(if_descriptor);
  FETCH(ep_descriptor_in);
  FETCH(ep_descriptor_out);

  FETCH(device_descriptor);

  // Configure ep0
  send_size = (uint32_t)cp-(uint32_t)init_config;
  ret = write(fd, init_config, send_size);

  if (ret != send_size) {
    printf("Write error %d (%m)\n", ret);
    return 2;
  }
  printf("ep0 configured %d\n", send_size);
  return 0;
}

int main()
{
    UsbGadget gadget;
    gadget.openGadget();

    gadget.handle_ep0();

    return 0;
}

void UsbGadget::handle_ep0() {
  int ret, nevents, i;
  fd_set read_set;
  struct usb_gadgetfs_event events[5];

  while (1) {
    printf("handle_ep0 %d\n", fd);
    FD_ZERO(&read_set);
    FD_SET(fd, &read_set);

    ret = select(fd+1, &read_set, NULL, NULL, NULL);
    printf("ep0 select returned %d\n", ret);
    if (FD_ISSET(fd, &read_set)) {
      ret = read(fd, &events, sizeof(events));

      if (ret < 0) {
        printf("ep0 Read error %d (%m)\n", ret);
        break;
      }

      nevents = ret / sizeof(events[0]);

      printf("%d event(s)\n", nevents);

      for (i=0; i<nevents; i++) {
        switch (events[i].type) {
        case GADGETFS_CONNECT:
            printf("EP0 CONNECT\n");
            break;
        case GADGETFS_DISCONNECT:
            printf("EP0 DISCONNECT\n");
            break;
        case GADGETFS_SETUP:
            printf("EP0 SETUP\n");
            handle_setup_request(fd, &events[i].u.setup);
            break;
        case GADGETFS_NOP:
        case GADGETFS_SUSPEND:
            break;
        }
      }
    }
  }
}

void UsbGadget::handle_setup_request(int fd, struct usb_ctrlrequest* setup) {
  int status, ret;
  uint8_t buffer[512];
  pthread_t thread;

  printf("  Setup request %d\n", setup->bRequest);

  switch (setup->bRequest) {
  case USB_REQ_GET_DESCRIPTOR:
    if (setup->bRequestType != USB_DIR_IN) goto stall;
    switch (setup->wValue >> 8) {
    case USB_DT_STRING:
      printf("    Get string id #%d (max length %d)\n", setup->wValue & 0xff, setup->wLength);
      status = usb_gadget_get_string (&strings, setup->wValue & 0xff, buffer);
      // Error
      if (status < 0) {
        printf("String not found !!\n");
        break;
      } else {
        printf("    Found %d bytes\n", status);
      }
      ret = write(fd, buffer, status);
      assert(ret == status);
      return;
    default:
      printf("Cannot return descriptor %d\n", (setup->wValue >> 8));
    }
    break;
  case USB_REQ_SET_CONFIGURATION:
    if (setup->bRequestType != USB_DIR_OUT) {
      printf("Bad dir\n");
      goto stall;
    }
    switch (setup->wValue) {
    case CONFIG_VALUE:
      printf("Set config value\n");
      if (!thread_args.stop) {
        thread_args.stop = 1;
        usleep(200000); // Wait for termination
      }
      if (thread_args.fd_in <= 0) {
        status = init_ep(&thread_args.fd_in, &thread_args.fd_out);
      } else {
        status = 0;
      }
      if (!status) {
        thread_args.stop = 0;
        pthread_create(&thread, NULL, io_thread, &thread_args);
      }
      break;
    case 0:
      printf("Disable threads\n");
      thread_args.stop = 1;
      break;
    default:
      printf("Unhandled configuration value %d\n", setup->wValue);
      break;
    }
    // Just ACK
    status = read(fd, &status, 0);
    return;
  case USB_REQ_GET_INTERFACE:
    printf("GET_INTERFACE\n");
    buffer[0] = 0;
    ret = write (fd, buffer, 1);
    assert(ret == 1);
    return;
  case USB_REQ_SET_INTERFACE:
    printf("SET_INTERFACE\n");
    ioctl(thread_args.fd_in, GADGETFS_CLEAR_HALT);
    ioctl(thread_args.fd_out, GADGETFS_CLEAR_HALT);
    // ACK
    status = read(fd, &status, 0);
    return;
  }

stall:
  printf("Stalled\n");
  // Error
  if (setup->bRequestType & USB_DIR_IN) {
    ret = read (fd, &status, 0);
    assert(ret == 0);
  } else {
    ret = write (fd, &status, 0);
    assert(ret == 0);
  }
}

int UsbGadget::init_ep(int* fd_in, int* fd_out) {
  uint8_t init_config[2048];
  uint8_t* cp;
  uint32_t ret = -1;
  uint32_t send_size;

  // Configure ep1 (low/full speed + high speed)
  *fd_in = open(USB_EPIN, O_RDWR);

  if (*fd_in <= 0) {
    printf("Unable to open %s (%m)\n", USB_EPIN);
    goto end;
  }

  *(uint32_t*)init_config = 1;
  cp = &init_config[4];

  FETCH(ep_descriptor_in);
  FETCH(ep_descriptor_in);

  send_size = (uint32_t)cp-(uint32_t)init_config;
  ret = write(*fd_in, init_config, send_size);

  if (ret != send_size) {
    printf("Write error %d (%m)\n", ret);
    goto end;
  }

  puts("ep1 configured");

  // Configure ep4 (low/full speed + high speed)
  *fd_out = open(USB_EPOUT, O_RDWR);

  if (*fd_out <= 0) {
    printf("Unable to open %s (%m)\n", USB_EPOUT);
    goto end;
  }

  *(uint32_t*)init_config = 1;
  cp = &init_config[4];

  FETCH(ep_descriptor_out);
  FETCH(ep_descriptor_out);

  send_size = (uint32_t)cp-(uint32_t)init_config;
  ret = write(*fd_out, init_config, send_size);

  if (ret != send_size) {
    printf("Write error %d (%m)\n", ret);
    goto end;
  }

  puts("ep4 configured");

  ret = 0;

end:
  return ret;
}

/*
 * Respond to host requests
 */
static void* io_thread(void* arg) {
  struct io_thread_args* thread_args = (struct io_thread_args*)arg;
  fd_set read_set;
  struct timeval timeout;
  int ret, max_read_fd, max_write_fd;
  char buffer[512];

  max_read_fd = max_write_fd = 0;

  if (thread_args->fd_in > max_write_fd) max_write_fd = thread_args->fd_in;
  if (thread_args->fd_out > max_read_fd) max_read_fd  = thread_args->fd_out;

  while (!thread_args->stop) {
    FD_ZERO(&read_set);
    FD_SET(thread_args->fd_out, &read_set);
    timeout.tv_sec = 0;
    timeout.tv_usec = 10000; // 10ms

    memset(buffer, 0, sizeof(buffer));
    ret = select(max_read_fd+1, &read_set, NULL, NULL, &timeout);
    printf("ep4 select returned %d\n", ret);

    // Timeout
    if (ret == 0) continue;

    // Error
    if (ret < 0) break;

    if (FD_ISSET(thread_args->fd_out, &read_set)) {
      ret = read (thread_args->fd_out, buffer, sizeof(buffer));

      if (ret > 0) {
        printf("  Read %d bytes : %s\n", ret, buffer);
        aa_conn.handle_incoming_frame(thread_args->fd_in, (uint8_t*)buffer, ret);
      } else {
        printf("  Read error %d(%m)\n", ret);
      }

      //FD_ZERO(&write_set);
      //FD_SET(thread_args->fd_in, &write_set);

      //memset(buffer, 0, sizeof(buffer));
      //ret = select(max_write_fd+1, NULL, &write_set, NULL, NULL);

      // Error
      //if (ret < 0)
      //    break;

      //strcpy(buffer, "My name is USBond !");

      //ret = write (thread_args->fd_in, buffer, strlen(buffer)+1);

      //printf("Write status %d (%m)\n", ret);
    }
    aa_conn.ssl.maybeSendOutgoingCiphertext(thread_args->fd_in);
  }

  close(thread_args->fd_in);
  close(thread_args->fd_out);

  thread_args->fd_in = -1;
  thread_args->fd_out = -1;

  return NULL;
}

