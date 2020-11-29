self: super: {
  sources = import ./sources.nix;
  baseFirmware = self.runCommand "base-firmware" {} ''
    mkdir $out
    cp -r ${self.sources.firmware}/boot/{*.dtb,kernel*img,fixup*dat,start*elf,overlays} $out/
  '';
  modulesForKernel = kernel: self.runCommand "modules" {} ''
    mkdir -pv $out/lib/modules/
    cp -r ${self.sources.firmware}/modules/${kernel} $out/lib/modules/
  '';
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
  etc = self.runCommand "etc" {} ''
    mkdir $out
    cd $out
    # sets password to password
    cat <<EOF > passwd
    root:nxz2xIegZ0Ytc:0:0:System administrator:/:/bin/ash
    EOF
  '';
  # 5.4.72-v7l+
  moduleClosureForKernel = kernel: self.makeModulesClosure {
    kernel = self.modulesForKernel kernel;
    firmware = "NONE";
    rootModules = [
      "dwc2"
      "usb_f_acm"
      "usb_f_rndis"
      "usb_f_mass_storage"
      "gadgetfs" # for custom userland gadgets
      "usb_f_hid"
    ];
  };
  installedPackages = self.buildEnv {
    name = "bin";
    paths = [
      self.shrunken_busybox self.boottime
      #self.gdb
    ] ++ self.extra_utils;
  };
  kernel_version = "5.4.72";
  kernel_versions = map (x: "${self.kernel_version}${x}") [ "+" "-v7+" "-v7l+" "-v8+" ];
  modulesForKernels = self.buildEnv {
    name = "all-the-modules";
    paths = map self.moduleClosureForKernel self.kernel_versions;
    pathsToLink = [ "/lib" ];
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
        symlink = "/bin";
        object = "${self.installedPackages}/bin";
      }
      {
        symlink = "/etc";
        object = self.etc;
      }
    ];
  };
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

      boottime

      depmod

      ${self.initrd_script}

      boottime

      exec ash
    '';
    executable = true;
  };
  rootDir = self.runCommand "rootdir" {} ''
    mkdir $out
    cd $out
    ln -s ${self.baseFirmware}/* .
    ln -s ${self.initrd}/initrd initrd
    ls -lLhs initrd
    cat <<EOF > config.txt
    dtoverlay=dwc2
    enable_uart=1
    uart_2ndstage=1
    dtoverlay=disable-bt

    initramfs initrd followkernel
    EOF

    cat <<EOF > cmdline.txt
    console=tty1 console=serial0,115200 quiet
    EOF
  '';
  rootZip = self.runCommand "rootzip" { nativeBuildInputs = [ self.buildPackages.zip ]; } ''
    cd ${self.rootDir}
    mkdir $out
    zip $out/root.zip *
    cd $out
    mkdir nix-support
    echo "file binary-dist $out/root.zip" > nix-support/hydra-build-products
  '';
}
