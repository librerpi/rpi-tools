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
  initScript = pkgs.writeTextFile {
    name = "init";
    text = ''
      #!${self.shrunken_busybox}/bin/ash

      mount -t proc proc proc
      mount -t sysfs sys sys
      mount -t devtmpfs dev dev

      truncate -s $((1024*1024*64)) disk.img

      modprobe -v dwc2
      modprobe -v g_mass_storage file=/disk.img removable=1 nofua=1
      exec ash
    '';
    executable = true;
  };
  moduleClosure = pkgs.makeModulesClosure {
    kernel = self.modulesDir;
    firmware = "NONE";
    rootModules = [ "dwc2" "g_mass_storage" ];
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
        object = "${self.shrunken_busybox}/bin";
      }
    ];
  };
  rootDir = pkgs.runCommand "rootdir" {} ''
    mkdir $out
    cd $out
    ln -sv ${self.baseFirmware}/* .
    ln -sv ${self.initrd}/initrd initrd
    cat <<EOF > config.txt
    uart_2ndstage=1
    enable_uart=1
    dtoverlay=disable-bt
    dtoverlay=dwc2

    initramfs initrd followkernel
    EOF

    cat <<EOF > cmdline.txt
    console=tty1 console=ttyAMA0,115200
    EOF
  '';
})
