{ config, pkgs, lib, initialPkgs, ... }:

with lib;

{
  options = {
    results = mkOption {
      internal = true;
      default = {};
      type = types.attrs;
    };
    utils = mkOption {
      default = {};
    };
    overlays = mkOption {
      default = [];
    };
    install.avahi = mkOption {
      type = types.bool;
      default = false;
    };
    install.openocd = mkOption {
      type = types.bool;
      default = false;
    };
    install.dbus = mkOption {
      type = types.bool;
      default = false;
    };
    install.systemd = mkOption {
      type = types.bool;
      default = false;
    };
    install.dropbear = mkOption {
      type = types.bool;
      default = false;
    };
    wifi = mkOption {
      type = types.bool;
      default = false;
    };
    trimRootDir = mkOption {
      default = "";
      type = types.lines;
    };
    initScript = mkOption {
    };
    initrd_script = mkOption {
      default = "";
      type = types.lines;
    };
    modulesForKernels = mkOption {
    };
    kernel_versions = mkOption {
      default = map (x: "${config.kernel_version}${x}") config.kernelVersionList;
    };
    kernel_version = mkOption {
      type = types.str;
      default = "5.10.16";
    };
    kernelVersionList = mkOption {
      default = [
        "+"     # pi0
        "-v7+"
        "-v7l+"
        "-v8+"
      ];
    };
    extra_modules = mkOption {
      default = [];
      type = types.listOf types.str;
    };
    installedPackages = mkOption {
    };
    extra_utils = mkOption {
      default = [];
    };
    shrunkenPackages = mkOption {
    };
    etc = mkOption {
    };
  };
  config = {
    _module.args = {
      pkgs = builtins.foldl' (super: super.extend) initialPkgs config.overlays;
    };
    modulesForKernels = pkgs.buildEnv {
      name = "all-the-modules";
      paths = (map config.utils.moduleClosureForKernel config.kernel_versions) ++ lib.optionals config.wifi [ pkgs.wireless-regdb pkgs.raspberrypiWirelessFirmware ];
      pathsToLink = [ "/lib" ];
      ignoreCollisions = true;
    };
    # see also:
    # https://elinux.org/images/e/ef/USB_Gadget_Configfs_API_0.pdf
    initScript = pkgs.writeTextFile {
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

        ${config.initrd_script}

        boottime

        exec ash
      '';
      executable = true;
    };
    overlays = [ (self: super: {
      smi-test = self.stdenv.mkDerivation {
        name = "smi-test";
        unpackPhase = ''
          cp ${../smi-test.c} smi-test.c
          export sourceRoot=.
        '';
        buildPhase = ''
          $CC smi-test.c -o smi-test -I ${self.linux_rpi0.src}/include/
        '';
        installPhase = ''
          mkdir -p $out/bin
          cp smi-test $out/bin
        '';
      };
      utillinux = super.utillinux.override { systemd = null; };
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
      baseFirmware = self.runCommand "base-firmware" {
        inherit (self) firmware;
      } ''
        mkdir $out
        cp -r $firmware/boot/{*.dtb,kernel*img,fixup*dat,start*elf,overlays} $out/
      '';
    }) ];
    etc = pkgs.callPackage ({ runCommand, nukeReferences }: runCommand "etc" {
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
      nativeBuildInputs = [ nukeReferences ];
    } ''
      mkdir -p $out/ssh
      cd $out
      ${lib.optionalString config.wifi ''
        cp ${../wpa_supplicant.conf} wpa_supplicant.conf
      ''}
      ${lib.optionalString config.install.avahi ''
        cp -r ${pkgs.avahi}/etc/avahi avahi
        chmod +w -R avahi
        for x in avahi/avahi-autoipd.action avahi/avahi-dnsconfd.action; do
          nuke-refs $x
        done
      ''}
      cp $nsswitchPath nsswitch.conf
      cp $passwdPath passwd
      cp $groupPath group
      cp $sshd_configPath ssh/sshd_config
      cp $nscdPath nscd.conf
    '') {};
    shrunkenPackages = pkgs.callPackage ({runCommandCC, nukeReferences }: runCommandCC "shrunken-packages" { nativeBuildInputs = [ nukeReferences ]; } ''
      mkdir -p $out/{bin,lib}
      ${lib.optionalString config.install.openocd ''
        cp ${pkgs.openocd}/bin/openocd $out/bin
      ''}
      ${lib.optionalString config.wifi ''
        cp ${pkgs.wpa_supplicant}/bin/wpa_supplicant $out/bin
      ''}
      ${lib.optionalString config.install.avahi ''
        cp ${pkgs.avahi}/bin/avahi-daemon $out/bin
      ''}
      cp ${pkgs.strace}/bin/strace $out/bin
      cp ${pkgs.openssh}/bin/sshd $out/bin
      #cp {pkgs.iproute}/bin/ip $out/bin
      ${lib.optionalString config.install.dropbear ''
        cp ${pkgs.dropbear}/bin/dropbear $out/bin/
      ''}
      cp ${pkgs.glibcCross.bin}/bin/nscd $out/bin
      cp ${pkgs.smi-test}/bin/smi-test $out/bin

      cp ${pkgs.hidapi}/lib/libhidapi-hidraw.so.0 $out/lib
      cp ${pkgs.libusb1}/lib/libusb-1.0.so.0 $out/lib
      cp ${pkgs.glibcCross}/lib/lib{m.so.6,dl.so.2,pthread.so.0,c.so.6,rt.so.1,util.so.1,crypt.so.1,resolv.so.2,nss_files.so.2} $out/lib
      cp $(cat $(cat $NIX_CC/nix-support/orig-cc)/nix-support/propagated-build-inputs)/${pkgs.stdenv.targetPlatform.config}/lib/lib{gcc_s.so.1,ssp.so.0} $out/lib/
      cp ${pkgs.utillinux.out}/lib/lib{mount.so.1,blkid.so.1,uuid.so.1} $out/lib/
      cp ${pkgs.openssl.out}/lib/lib{ssl.so.1.1,crypto.so.1.1} $out/lib
      cp ${pkgs.libnl.out}/lib/li{bnl-3.so.200,bnl-genl-3.so.200} $out/lib
      ${lib.optionalString config.wifi ''
        cp ${pkgs.pcsclite.out}/lib/libpcsclite.so.1 $out/lib
      ''}
      ${lib.optionalString config.install.dbus ''
        cp ${pkgs.dbus.lib}/lib/libdbus-1.so.3 $out/lib
      ''}
      ${lib.optionalString config.install.systemd ''
        cp ${pkgs.systemd}/lib/libsystemd.so.0 $out/lib
        cp ${pkgs.udev}/lib/libudev.so.1 $out/lib
      ''}
      cp ${pkgs.xz.out}/lib/liblzma.so.5 $out/lib
      cp ${pkgs.lz4.out}/lib/liblz4.so.1 $out/lib
      cp ${pkgs.libcap.lib}/lib/libcap.so.2 $out/lib
      cp ${pkgs.libgcrypt.out}/lib/libgcrypt.so.20 $out/lib
      cp ${pkgs.libgpgerror}/lib/libgpg-error.so.0 $out/lib
      ${lib.optionalString config.install.avahi ''
        cp ${pkgs.avahi}/lib/lib{avahi-common.so.3,avahi-core.so.7} $out/lib
      ''}
      cp ${pkgs.libdaemon}/lib/libdaemon.so.0 $out/lib
      cp ${pkgs.expat}/lib/libexpat.so.1 $out/lib
      cp ${pkgs.libunwind}/lib/lib{unwind-ptrace.so.0,unwind-arm.so.8,unwind.so.8} $out/lib
      cp ${pkgs.pam}/lib/libpam.so.0 $out/lib
      cp ${pkgs.zlib}/lib/libz.so.1 $out/lib
      cp ${pkgs.libkrb5}/lib/lib{gssapi_krb5.so.2,krb5.so.3,k5crypto.so.3,com_err.so.3,krb5support.so.0} $out/lib
      cp ${pkgs.keyutils.lib}/lib/libkeyutils.so.1 $out/lib

      linker=$(basename $(cat $NIX_CC/nix-support/dynamic-linker))
      chmod +w -R $out

      ${lib.optionalString config.install.openocd ''
        sed -i -e 's@${pkgs.openocd}@/nix/store/eeeeeeeeeeeeeeeeeeeeeeeeeeeeeeee-${pkgs.openocd.name}@' $out/bin/openocd
        sed -i -e 's@${pkgs.openocd}@/nix/store/eeeeeeeeeeeeeeeeeeeeeeeeeeeeeeee-${pkgs.openocd.name}@' $out/bin/openocd
      ''}
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
      sed -i -e 's@${pkgs.glibcCross.out}@/nix/store/eeeeeeeeeeeeeeeeeeeeeeeeeeeeeeee-${pkgs.glibcCross.name}@' $out/lib/$linker
    '') {};
    installedPackages = pkgs.buildEnv {
      name = "bin";
      paths = [
        pkgs.shrunken_busybox
        #self.boottime
        #self.gdb
        config.shrunkenPackages
      ] ++ config.extra_utils;
    };
    utils = {
      modulesForKernel = kernel: pkgs.runCommand "modules-${kernel}" {} ''
        mkdir -pv $out/lib/modules/
        cp -r ${pkgs.firmware}/modules/${kernel} $out/lib/modules/
      '';
      moduleClosureForKernel = kernel: pkgs.makeModulesClosure {
        kernel = config.utils.modulesForKernel kernel;
        firmware = pkgs.buildEnv {
          name = "all-the-firmware";
          paths = [ pkgs.firmwareLinuxNonfree pkgs.raspberrypiWirelessFirmware ];
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
        ] ++ config.extra_modules;
      };
    };
    results.closure = pkgs.runCommand "closure-helper" {} ''
      mkdir $out
      cd $out
      ln -s ${config.initScript} init
      ln -s ${config.modulesForKernels} modules
      ln -s ${config.installedPackages} bin
      ln -s ${config.etc} etc
    '';
    results.initrd = pkgs.makeInitrd {
      contents = [
        {
          symlink = "/init";
          object = config.initScript;
        }
        {
          symlink = "/lib/modules";
          object = "${config.modulesForKernels}/lib/modules";
        }
        {
          symlink = "/lib/firmware";
          object = "${config.modulesForKernels}/lib/firmware";
        }
        {
          symlink = "/bin";
          object = "${config.installedPackages}/bin";
        }
        {
          symlink = "/etc";
          object = config.etc;
        }
      ];
    };
    results.rootDir = pkgs.runCommand "rootdir" {} ''
      mkdir $out
      cd $out
      ln -s ${pkgs.baseFirmware}/* .
      ln -s ${config.results.initrd}/initrd initrd
      ls -lLhs initrd
      cat <<EOF > config.txt
      dtoverlay=dwc2
      dtoverlay=smi
      dtoverlay=smi-dev
      enable_uart=1
      uart_2ndstage=1
      dtoverlay=disable-bt

      initramfs initrd followkernel
      EOF

      cat <<EOF > cmdline.txt
      nada console=tty1 console=serial0,115200
      EOF

      ${config.trimRootDir}
    '';
  };
}
