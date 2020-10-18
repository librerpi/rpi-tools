a webusb implementation of https://github.com/raspberrypi/usbboot

only tested on an rpi4

click `check for pi` to discover a connected pi, and authorize the JS to control it

click `push bootcode` to push the currently selected .bin file over

click `refetch lk.bin` to download another lk.bin file

click `fetch recovery.bin` to switch over to `recovery.bin` (usbboot must be cloned to this dir)

if `auto push bootcode` is checked, it will push the last fetched .bin upon detecting a usb device

also implements the fileserver used by the official firmware
