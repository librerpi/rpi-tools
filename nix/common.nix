let
  sources = import ./sources.nix;
  nativePkgs = import sources.nixpkgs {};
  master = import (builtins.fetchTarball https://github.com/nixos/nixpkgs/archive/master.tar.gz) {};
in
self: super: {
  inherit sources;
  baseFirmware = self.runCommand "base-firmware" {} ''
    mkdir $out
    cp -r ${self.sources.firmware}/boot/{*.dtb,kernel*img,fixup*dat,start*elf,overlays} $out/
  '';
  modulesForKernel = kernel: self.runCommand "modules" {} ''
    mkdir -pv $out/lib/modules/
    cp -r ${self.sources.firmware}/modules/${kernel} $out/lib/modules/
  '';
  libftdi = null;
  openocd = super.openocd.overrideAttrs (old: {
    src = self.fetchFromGitHub {
      owner = "raspberrypi";
      repo = "openocd";
      rev = "14c0d0d330bd6b2cdc0605ee9a9256e5627a905e";
      fetchSubmodules = true;
      sha256 = "sha256-o7shTToj6K37Xw+Crwif5WwB4GfPYIiMJ/o/9u3xrsE=";
    };
    nativeBuildInputs = old.nativeBuildInputs ++ [ nativePkgs.autoreconfHook nativePkgs.gcc ];
    #buildInputs = old.buildInputs ++ [ self.tcl ];
    preConfigure = ''
      pwd
      ls -l
    '';
    configureFlags = [
      "--enable-bcm2835gpio"
      "--enable-sysfsgpio"
    ];
  });
  shrunken_busybox = self.runCommand "shrunk-busybox" {
    busybox = self.busybox.override { enableStatic=true; };
    nativeBuildInputs = [ self.buildPackages.nukeReferences ];
  } ''
    mkdir $out
    cp -vir $busybox/bin $out/
    chmod +w $out/bin
    chmod +w $out/bin/busybox
    nuke-refs $out/bin/busybox
  '';
  boottime = self.stdenv.mkDerivation {
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
  rpi-tools = self.lib.makeScope self.newScope (iself: {
    utils = iself.callPackage ../utils {};
    tlsf = null;
    common = iself.callPackage "${self.sources.rpi-open-firmware}/common" {};
  });
  withWifi = false;
  etc = self.runCommand "etc" {
    nsswitch = ''
      passwd:    files systemd
      group:     files systemd
      shadow:    files

      hosts:     files mymachines mdns_minimal [NOTFOUND=return] dns mdns myhostname
      networks:  files

      ethers:    files
      services:  files
      protocols: files
      rpc:       files
    '';
    # sets root password to password
    passwd = ''
      root:nxz2xIegZ0Ytc:0:0:System administrator:/:/bin/sh
      avahi:x:10:10:avahi-daemon privilege separation user:/var/empty:/run/current-system/sw/bin/nologin
      sshd:x:498:65534:SSH privilege separation user:/var/empty:/run/current-system/sw/bin/nologin
      nscd:x:2:2:nscd privilege separation user:/var/empty:/run/current-system/sw/bin/nologin
    '';
    group = ''
      avahi:x:10:
    '';
    sshd_config = ''
      UsePAM no
      Port 22
    '';
    nscd = ''
      server-user             nscd

      enable-cache            passwd          yes
      positive-time-to-live   passwd          0
      negative-time-to-live   passwd          0
      shared                  passwd          yes

      enable-cache            group           yes
      positive-time-to-live   group           0
      negative-time-to-live   group           0
      shared                  group           yes

      enable-cache            netgroup        yes
      positive-time-to-live   netgroup        0
      negative-time-to-live   netgroup        0
      shared                  netgroup        yes

      enable-cache            hosts           yes
      positive-time-to-live   hosts           0
      negative-time-to-live   hosts           0
      shared                  hosts           yes

      enable-cache            services        yes
      positive-time-to-live   services        0
      negative-time-to-live   services        0
      shared                  services        yes
    '';
    passAsFile = [ "nsswitch" "passwd" "sshd_config" "nscd" "group" ];
    nativeBuildInputs = [ nativePkgs.nukeReferences ];
  } ''
    mkdir -p $out/ssh
    cd $out
    ${self.lib.optionalString self.withWifi ''
      cp ${../wpa_supplicant.conf} wpa_supplicant.conf
    ''}
    cp -r ${self.avahi}/etc/avahi avahi
    chmod +w -R avahi
    for x in avahi/avahi-autoipd.action avahi/avahi-dnsconfd.action; do
      nuke-refs $x
    done
    cp $nsswitchPath nsswitch.conf
    cp $passwdPath passwd
    cp $groupPath group
    cp $sshd_configPath ssh/sshd_config
    cp $nscdPath nscd.conf
  '';
  # 5.4.72-v7l+
  moduleClosureForKernel = kernel: self.makeModulesClosure {
    kernel = self.modulesForKernel kernel;
    firmware = self.buildEnv {
      name = "all-the-firmware";
      paths = [ self.firmwareLinuxNonfree master.raspberrypiWirelessFirmware ];
      ignoreCollisions = true;
    };
    allowMissing = true;
    rootModules = [
      "dwc2"
      "usb_f_acm"
      "usb_f_rndis"
      "usb_f_mass_storage"
      "gadgetfs" # for custom userland gadgets
      "usb_f_hid"
      "usb_f_rndis"
    ] ++ self.extra_modules;
  };
  extra_modules = [];
  installedPackages = self.buildEnv {
    name = "bin";
    paths = [
      self.shrunken_busybox
      #self.boottime
      #self.gdb
      self.shrunkenPackages
    ] ++ self.extra_utils;
  };
  libnl = super.libnl.override { pythonSupport = false; };
  shrunkenPackages = self.runCommandCC "shrunken-packages" { nativeBuildInputs = [ nativePkgs.nukeReferences ]; } ''
    mkdir -p $out/{bin,lib}
    cp ${self.openocd}/bin/openocd $out/bin
    cp ${self.wpa_supplicant}/bin/wpa_supplicant $out/bin
    cp ${self.avahi}/bin/avahi-daemon $out/bin
    cp ${self.strace}/bin/strace $out/bin
    cp ${self.openssh}/bin/sshd $out/bin
    #cp {self.iproute}/bin/ip $out/bin
    cp ${self.dropbear}/bin/dropbear $out/bin/
    cp ${self.glibcCross.bin}/bin/nscd $out/bin
    #cp {self.smi-test}/bin/smi-test $out/bin

    cp ${self.hidapi}/lib/libhidapi-hidraw.so.0 $out/lib
    cp ${self.libusb1}/lib/libusb-1.0.so.0 $out/lib
    cp ${self.glibcCross}/lib/lib{m.so.6,dl.so.2,pthread.so.0,c.so.6,rt.so.1,util.so.1,crypt.so.1,resolv.so.2,nss_files.so.2} $out/lib
    cp ${self.udev.lib}/lib/libudev.so.1 $out/lib
    cp $(cat $(cat $NIX_CC/nix-support/orig-cc)/nix-support/propagated-build-inputs)/lib/lib{gcc_s.so.1,ssp.so.0} $out/lib/
    cp ${self.utillinux.out}/lib/lib{mount.so.1,blkid.so.1,uuid.so.1} $out/lib/
    cp ${self.openssl.out}/lib/lib{ssl.so.1.0.0,crypto.so.1.0.0} $out/lib
    cp ${self.libnl.out}/lib/li{bnl-3.so.200,bnl-genl-3.so.200} $out/lib
    cp ${self.pcsclite.out}/lib/libpcsclite.so.1 $out/lib
    cp ${self.dbus.lib}/lib/libdbus-1.so.3 $out/lib
    cp ${self.systemd.lib}/lib/libsystemd.so.0 $out/lib
    cp ${self.xz.out}/lib/liblzma.so.5 $out/lib
    cp ${self.lz4.out}/lib/liblz4.so.1 $out/lib
    cp ${self.libcap.lib}/lib/libcap.so.2 $out/lib
    cp ${self.libgcrypt.out}/lib/libgcrypt.so.20 $out/lib
    cp ${self.libgpgerror}/lib/libgpg-error.so.0 $out/lib
    cp ${self.avahi}/lib/lib{avahi-common.so.3,avahi-core.so.7} $out/lib
    cp ${self.libdaemon}/lib/libdaemon.so.0 $out/lib
    cp ${self.expat}/lib/libexpat.so.1 $out/lib
    cp ${self.libunwind}/lib/lib{unwind-ptrace.so.0,unwind-arm.so.8,unwind.so.8} $out/lib
    cp ${self.pam}/lib/libpam.so.0 $out/lib
    cp ${self.zlib}/lib/libz.so.1 $out/lib
    cp ${self.libkrb5}/lib/lib{gssapi_krb5.so.2,krb5.so.3,k5crypto.so.3,com_err.so.3,krb5support.so.0} $out/lib
    cp ${self.keyutils.lib}/lib/libkeyutils.so.1 $out/lib

    linker=$(basename $(cat $NIX_CC/nix-support/dynamic-linker))
    chmod +w -R $out

    sed -i -e 's@${self.openocd}@/nix/store/eeeeeeeeeeeeeeeeeeeeeeeeeeeeeeee-${self.openocd.name}@' $out/bin/openocd
    sed -i -e 's@${self.openocd}@/nix/store/eeeeeeeeeeeeeeeeeeeeeeeeeeeeeeee-${self.openocd.name}@' $out/bin/openocd
    for x in $out/lib/lib{pthread.so.0,gcc_s.so.1,rt.so.1,mount.so.1,crypto.so.1.0.0,dbus-1.so.3,gpg-error.so.0,ssp.so.0,daemon.so.0,avahi-common.so.3,pam.so.0,util.so.1,z.so.1,crypt.so.1,resolv.so.2,gssapi_krb5.so.2,krb5.so.3,k5crypto.so.3,com_err.so.3,nss_files.so.2}; do
      nuke-refs $x
    done
    nuke-refs $out/bin/avahi-daemon
    nuke-refs $out/bin/sshd
    nuke-refs $out/bin/nscd

    for bin in $out/bin/*; do
      patchelf --set-rpath $out/lib --set-interpreter $out/lib/$linker $bin
    done
    for lib in $out/lib/*; do
      patchelf --set-rpath $out/lib $lib
    done

    cp $(cat $NIX_CC/nix-support/dynamic-linker) $out/lib/$linker
    chmod +w -R $out
    $STRIP $out/lib/$linker
    nuke-refs $out/lib/libc.so*
    nuke-refs $out/lib/libdl.so.2
    nuke-refs $out/lib/libm.so.6
    sed -i -e 's@${self.glibcCross.out}@/nix/store/eeeeeeeeeeeeeeeeeeeeeeeeeeeeeeee-${self.glibcCross.name}@' $out/lib/$linker
  '';
  trimRootDir = ''
    rm kernel8.img kernel7.img kernel.img
    rm start.elf start*db.elf start*x.elf start*cd.elf
    rm fixup.dat fixup*db.dat fixup*x.dat fixup*cd.dat
    rm bcm{2708,2710,2709}*dtb
  '';
  kernel_version = "5.4.72";
  kernelVersionList = [
    "+"     # pi0
    "-v7+"
    "-v7l+"
    "-v8+"
  ];
  kernel_versions = map (x: "${self.kernel_version}${x}") self.kernelVersionList;
  modulesForKernels = self.buildEnv {
    name = "all-the-modules";
    paths = (map self.moduleClosureForKernel self.kernel_versions) ++ [ self.wireless-regdb self.raspberrypiWirelessFirmware ];
    pathsToLink = [ "/lib" ];
    ignoreCollisions = true;
  };
  initrd = self.makeInitrd {
    contents = [
      {
        symlink = "/init";
        object = self.initScript;
      }
      {
        symlink = "/lib/modules";
        object = "${self.modulesForKernels}/lib/modules";
      }
      {
        symlink = "/lib/firmware";
        object = "${self.modulesForKernels}/lib/firmware";
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
  closure = self.runCommand "closure-helper" {} ''
    mkdir $out
    cd $out
    ln -s ${self.initScript} init
    ln -s ${self.modulesForKernels} modules
    ln -s ${self.installedPackages} bin
    ln -s ${self.etc} etc
  '';
  # see also:
  # https://elinux.org/images/e/ef/USB_Gadget_Configfs_API_0.pdf
  initScript = self.writeTextFile {
    name = "init";
    text = ''
      #!/bin/ash

      mount -t proc proc proc
      mount -t sysfs sys sys
      mount -t devtmpfs dev dev
      mount -t configfs none /sys/kernel/config
      mkdir /dev/pts
      mount -t devpts devpts /dev/pts
      mount -t debugfs debugfs /sys/kernel/debug

      boottime

      depmod
      serial=$(cut -c9-16 < /proc/device-tree/serial-number)
      hostname pi-''${serial}

      ${self.initrd_script}

      boottime

      exec ash
    '';
    executable = true;
  };
  custom-overlays = self.callPackage ({ runCommand, dtc, }: runCommand "custom-overlays" { nativeBuildInputs = [ dtc ]; } ''
    mkdir -p $out/overlays
    cd $out/overlays
    dtc -@ -Hepapr -I dts -O dtb -o smi-speed.dtbo ${../smi-speed-overlay.dts}
  '') {};
  firmware-with-custom-overlays = self.buildEnv {
    name = "firmware-with-custom-overlays";
    paths = [ self.baseFirmware self.custom-overlays ];
  };
  rootDir = self.runCommand "rootdir" {} ''
    mkdir $out
    cd $out
    ln -s ${self.firmware-with-custom-overlays}/* .
    ln -s ${self.initrd}/initrd initrd
    ls -lLhs initrd
    cat <<EOF > config.txt
    dtoverlay=dwc2
    dtoverlay=smi
    dtoverlay=smi-dev
    dtoverlay=smi-speed
    dtparam=axiperf
    enable_uart=1
    uart_2ndstage=1
    dtoverlay=disable-bt

    initramfs initrd followkernel
    EOF

    cat <<EOF > cmdline.txt
    nada console=tty1 console=serial0,115200
    EOF

    ${self.trimRootDir}
  '';
  util = self.runCommand "util" {} ''
    mkdir $out
    cd $out
    ln -sv ${self.rootDir} rootdir
    ln -sv ${self.closure} closure
  '';
  util2 = (import <nixpkgs> {system="aarch64-linux";}).runCommand "util2" {} ''
    ${self.shrunkenPackages}/bin/strace --help
    touch $out
  '';
  rootZip = self.runCommand "rootzip" { nativeBuildInputs = [ self.buildPackages.zip ]; } ''
    cd ${self.rootDir}
    mkdir $out
    zip -r $out/root.zip *
    cd $out
    mkdir nix-support
    echo "file binary-dist $out/root.zip" > nix-support/hydra-build-products
  '';
}
