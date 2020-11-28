pself: psuper: {
  msd = pself.extend (self: super: {
    extra_utils = [];
    initrd_script = ''
      truncate -s $((1024*1024*64)) disk.img
      modprobe -v dwc2
      modprobe -v usb_f_acm
      modprobe -v usb_f_mass_storage

      cd /sys/kernel/config/usb_gadget
      mkdir g1
      cd g1

      mkdir functions/acm.GS0

      mkdir functions/mass_storage.GS0
      echo 1 > functions/mass_storage.GS0/lun.0/removable
      if [ -e /dev/mmcblk0 ]; then
        echo /dev/mmcblk0 > functions/mass_storage.GS0/lun.0/file
      else
        echo /disk.img > functions/mass_storage.GS0/lun.0/file
      fi

      mkdir configs/c.1
      mkdir configs/c.1/strings/0x409
      echo "Serial Console + MSD" > configs/c.1/strings/0x409/configuration

      mkdir strings/0x409
      echo "cleverca22" > strings/0x409/manufacturer
      echo "rpi-tools" > strings/0x409/product
      grep Serial /proc/cpuinfo  | cut -c19-26 > strings/0x409/serialnumber

      ln -sv functions/acm.GS0 configs/c.1
      ln -sv functions/mass_storage.GS0 configs/c.1

      echo fe980000.usb > UDC

      cd /
      getty 0 /dev/ttyGS0 &
    '';
  });
}
