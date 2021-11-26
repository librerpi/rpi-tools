{ nixpkgs-be }:

let
  pkgs = import nixpkgs-be { system = "x86_64-linux"; };
  hello = pkgs.pkgsCross.aarch64be-multiplatform.hello;
  oldinitrd = pkgs.makeInitrd {
    contents = [
      {
        symlink = "/init";
        object = "${hello}/bin/hello";
      }
    ];
  };
  eval = import (nixpkgs-be + "/nixos") { system = "x86_64-linux"; configuration = ./configuration.nix; };
  diskImage = pkgs.callPackage (nixpkgs-be + "/nixos/lib/make-ext4-fs.nix") {
    storePaths = [ eval.system ];
    volumeLabel = "nixos";
  };
  kernel = "${eval.config.system.build.kernel}/${eval.config.system.boot.loader.kernelFile}";
  initrd = "${eval.config.system.build.initialRamdisk}/initrd";
  script = (pkgs.writeShellScript "runner" ''
    export PATH=${pkgs.lib.makeBinPath [ pkgs.qemu ]}
    qemu-system-aarch64 -kernel ${kernel} \
        -append '${builtins.unsafeDiscardStringContext (toString (eval.config.boot.kernelParams))} root=/dev/vda init=${builtins.unsafeDiscardStringContext eval.config.system.build.toplevel}/init console=ttyAMA0' \
        -initrd ${initrd} \
        -machine virt -serial stdio -cpu cortex-a72 \
        -drive id=rootfs,if=virtio,snapshot=on,format=raw,file=${diskImage} \
        -nic user,hostfwd=tcp:127.0.0.2:2222-:22,model=virtio-net-pci \
        -m 1024
  '') // { inherit eval; };
in {
  inherit script diskImage eval;
}
