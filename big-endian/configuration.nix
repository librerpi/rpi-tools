{ pkgs, lib, ... }:

let
  patchedKernel = pkgs.linux_rpi3.override {
    structuredExtraConfig = with lib.kernel; {
      BLK_MQ_VIRTIO = yes;
      CPU_BIG_ENDIAN = yes;
      CRYPTO_AEGIS128_SIMD = no;
      PCI = yes;
      PCI_HOST_GENERIC = yes;
      SCSI_VIRTIO = yes;
      VIRTIO = yes;
      VIRTIO_BLK = yes;
      VIRTIO_MENU = yes;
      VIRTIO_MMIO = yes;
      VIRTIO_NET = yes;
      VIRTIO_PCI = yes;
      VIRTIO_PCI_LEGACY = yes;
    };
  };
  patchedKernelPackages = pkgs.linuxPackagesFor patchedKernel;
in {
  nixpkgs.crossSystem.system = "aarch64_be-linux";
  boot = {
    kernelPackages = patchedKernelPackages;
    isContainer = false;
    consoleLogLevel = 8;
  };
  boot.loader.grub.enable = false;
  fileSystems = {
    "/" = {
      label = "nixos";
    };
  };
  nixpkgs.overlays = [ (import ./overlay.nix) ];
  security.polkit.enable = false;
  services.udisks2.enable = false;
  users.users.root = {
    initialPassword = "root";
  };
  services.openssh.enable = true;
  services.openssh.permitRootLogin = "yes";
}
