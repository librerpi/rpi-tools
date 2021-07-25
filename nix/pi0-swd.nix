pself: psuper: {
  pi0-swd = (import pself.path { overlays = [ (import ./common.nix) ]; }).pkgsCross.raspberryPi.extend (self: super: {
    initrd_script = ''
      modprobe brcmfmac
      modprobe bcm2835_smi_dev
      sleep 10
      wpa_supplicant -iwlan0 -Dnl80211,wext -c /etc/wpa_supplicant.conf &
      sleep 5
      ip addr add 192.168.2.50 dev wlan0
      ip route add 192.168.2.0/24 dev wlan0
      mkdir -pv /etc/dropbear /var/log
      touch /var/log/lastlog
      /bin/dropbear -R -E &
      mkdir -pv /var/run/nscd
      LD_LIBRARY_PATH=${self.shrunkenPackages}/lib nscd &
      mkdir -p /run/avahi-daemon
    '';
    #systemd = self.eudev;
    #systemdMinimal = null;
    #libusb = super.libusb.override { enableUdev = false; };
    #pcsclite = super.pcsclite.overrideAttrs (old: { configureFlags = old.configureFlags ++ [ "--disable-libsystemd" "--disable-libudev" ]; });
    wpa_supplicant = super.wpa_supplicant.overrideAttrs (old: {
      buildInputs = old.buildInputs ++ [ self.libusb ];
    });
    ocd-scripts = self.runCommand "ocd-scripts" {} ''
      cp -vir ${self.openocd}/share/openocd/scripts/ $out
    '';
    startOcd = self.writeScriptBin "start-ocd" ''
      #!/bin/sh
      openocd -f interface/raspberrypi-swd.cfg -f target/rp2040.cfg -s ${self.ocd-scripts}
    '';
    extra_utils = with self; [
      #wpa_supplicant
      #openocd
      self.startOcd
    ];
    trimRootDir = ''
      rm start4*
      rm fixup4*
      rm bcm2711* bcm2710* bcm2709*
      rm kernel8.img kernel7l.img kernel7.img
    '';
    extra_modules = [
      "brcmfmac"
      "configs"
      "bcm2835_smi"
      "bcm2835_smi_dev"
      "raspberrypi_axi_monitor"
    ];
    kernelVersionList = [ "+" ];
    smi-test = self.stdenv.mkDerivation {
      name = "smi-test";
      unpackPhase = ''
        cp ${../smi-test.c} smi-test.c
        export sourceRoot=.
      '';
      buildPhase = ''
        $CC smi-test.c -o smi-test -I ${self.linux_rpi.src}/include/
      '';
      installPhase = ''
        mkdir -p $out/bin
        cp smi-test $out/bin
      '';
    };
  });
}
