{ config, lib, pkgs, ... }:

with lib;
{
  options = {
    pi400-keyboard = lib.mkOption {
      type = types.bool;
      default = true;
    };
  };
  config = lib.mkIf config.pi400-keyboard {
    overlays = [
      (self: super: {
        sendkey = self.writeScriptBin "sendkey" ''
          #!/bin/ash
          echo -ne \\x00\\x00\\x''${1}\\x00\\x00\\x00\\x00\\x00\\x00\\x00\\x00\\x00\\x00\\x00\\x00\\x00 > /dev/hidg0
        '';
        keyboard_proxy = self.stdenv.mkDerivation {
          name = "keyboard-proxy";
          src = ../keyboard-proxy;
          buildPhase = ''
            $CC main.c -o keyboard-proxy -Wall
          '';
          installPhase = ''
            mkdir -pv $out/bin/
            cp keyboard-proxy $out/bin/
          '';
        };
      })
    ];
    extra_utils = [ pkgs.sendkey pkgs.keyboard_proxy ];
    initrd_script = ''
      modprobe -v dwc2
      modprobe usb_f_hid

      cd /sys/kernel/config/usb_gadget
      mkdir g1
      cd g1

      mkdir functions/hid.foo
      cd functions/hid.foo
      echo 1 > protocol
      echo 1 > subclass
      echo 8 > report_length
      echo -ne \\x05\\x01\\x09\\x06\\xa1\\x01\\x05\\x07\\x19\\xe0\\x29\\xe7\\x15\\x00\\x25\\x01\\x75\\x01\\x95\\x08\\x81\\x02\\x95\\x01\\x75\\x08\\x81\\x03\\x95\\x05\\x75\\x01\\x05\\x08\\x19\\x01\\x29\\x05\\x91\\x02\\x95\\x01\\x75\\x03\\x91\\x03\\x95\\x06\\x75\\x08\\x15\\x00\\x25\\x65\\x05\\x07\\x19\\x00\\x29\\x65\\x81\\x00\\xc0 > report_desc
      cd ../..

      mkdir configs/c.1
      mkdir configs/c.1/strings/0x409
      echo "keyboard mode" > configs/c.1/strings/0x409/configuration

      mkdir strings/0x409
      echo "cleverca22" > strings/0x409/manufacturer
      echo "rpi-tools pi400 keyboard" > strings/0x409/product
      grep Serial /proc/cpuinfo  | cut -c19-26 > strings/0x409/serialnumber

      ln -sv functions/hid.foo configs/c.1

      echo fe980000.usb > UDC

      cd /
      sleep 5
      keyboard-proxy &
    '';
  };
}
