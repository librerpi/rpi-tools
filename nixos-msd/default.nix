let
  sources = import ./nix/sources.nix;
  pkgs = import sources.nixpkgs {};
in pkgs.lib.fix (self: {
  baseFirmware = pkgs.runCommand "base-firmware" {} ''
    mkdir $out
    cp -r ${sources.firmware}/boot/{*.dtb,kernel*img,fixup*dat,start*elf,overlays} $out/
  '';
  modulesDir = pkgs.runCommand "modules" {} ''
    mkdir -pv $out/lib/modules/
    cp -r ${sources.firmware}/modules/5.4.72-v7l+ $out/lib/modules/
  '';
  shrunken_busybox = pkgs.runCommand "shrunk-busybox" {
    busybox = pkgs.pkgsCross.armv7l-hf-multiplatform.busybox.override { enableStatic=true; };
    buildInputs = [ pkgs.nukeReferences ];
  } ''
    mkdir $out
    cp -vir $busybox/bin $out/
    chmod +w $out/bin
    chmod +w $out/bin/busybox
    nuke-refs $out/bin/busybox
  '';
  boottime = pkgs.pkgsCross.armv7l-hf-multiplatform.pkgsStatic.stdenv.mkDerivation {
    name = "boottime";
    unpackPhase = ''
      cp ${../boottime.c} boottime.c
      export sourceRoot=.
    '';
    buildPhase = ''
      $CC boottime.c -o boottime
    '';
    installPhase = ''
      mkdir -p $out/bin
      cp boottime $out/bin/
    '';
  };
  # see also:
  # https://elinux.org/images/e/ef/USB_Gadget_Configfs_API_0.pdf
  initScript = pkgs.writeTextFile {
    name = "init";
    text = ''
      #!${self.shrunken_busybox}/bin/ash

      mount -t proc proc proc
      mount -t sysfs sys sys
      mount -t devtmpfs dev dev
      mount -t configfs none /sys/kernel/config

      boottime

      truncate -s $((1024*1024*64)) disk.img
      depmod

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

      boottime

      exec ash
    '';
    executable = true;
  };
  etc = pkgs.runCommand "etc" {} ''
    mkdir $out
    cd $out
    # sets password to password
    cat <<EOF > passwd
    root:nxz2xIegZ0Ytc:0:0:System administrator:/:/bin/ash
    EOF
  '';
  moduleClosure = pkgs.makeModulesClosure {
    kernel = self.modulesDir;
    firmware = "NONE";
    rootModules = [
      "dwc2"
      "usb_f_acm"
      "usb_f_rndis"
      "usb_f_mass_storage"
    ];
  };
  installedPackages = pkgs.buildEnv {
    name = "bin";
    paths = [ self.shrunken_busybox self.boottime ];
  };
  initrd = pkgs.makeInitrd {
    contents = [
      {
        symlink = "/init";
        object = self.initScript;
      }
      {
        symlink = "/lib/modules";
        object = "${self.moduleClosure}/lib/modules";
      }
      {
        symlink = "/bin";
        object = "${self.installedPackages}/bin";
      }
      {
        symlink = "/etc";
        object = self.etc;
      }
    ];
  };
  rootDir = pkgs.runCommand "rootdir" {} ''
    mkdir $out
    cd $out
    ln -sv ${self.baseFirmware}/* .
    ln -sv ${self.initrd}/initrd initrd
    cat <<EOF > config.txt
    dtoverlay=dwc2

    initramfs initrd followkernel
    EOF

    cat <<EOF > cmdline.txt
    console=tty1
    EOF
  '';
})
