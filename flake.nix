{
  inputs = {
    utils.url = "github:numtide/flake-utils";
    rpi-eeprom = {
      url = "github:raspberrypi/rpi-eeprom";
      flake = false;
    };
    rpi-open-firmware = {
      url = "github:librerpi/rpi-open-firmware";
      flake = false;
    };
    firmware = {
      url = "github:raspberrypi/firmware";
      flake = false;
    };
    nixpkgs.url = "path:/home/clever/apps/rpi/nixpkgs-test";
  };
  outputs = { self, nixpkgs, utils, rpi-eeprom, rpi-open-firmware, firmware }:
  utils.lib.eachSystem [ "x86_64-linux" "armv7l-linux" "armv6l-linux" "aarch64-linux" ] (system:
  let
    overlay = self: super: {
      inherit rpi-eeprom firmware;
      eeprom-extractor = self.callPackage ./eeprom {};
      extracted = self.callPackage ./extractor.nix {};
      utils = self.callPackage ./utils {};
      common = self.callPackage "${rpi-open-firmware}/common" {};
      tlsf = null;
      initrd_basic = self.callPackage ./initrd.nix {};
      e2fsprogs = null;
      zstd = null;
      valgrind = super.valgrind.overrideAttrs (old: {
        patches = [ ./valgrind.patch ];
      });
      valgrind-light = super.valgrind-light.overrideAttrs (old: {
        patches = [ ./valgrind.patch ];
      });
    };
    systemTable = {
      x86_64-linux = "x86_64-linux";
      armv7l-linux = "armv7l-linux";
      armv6l-linux = "armv6l-linux";
      aarch64-linux = "aarch64-linux";
    };
    crosser = {
      x86_64-linux = x: x;
      armv6l-linux = x: x;
      armv7l-linux = x: x;
      aarch64-linux = x: x;
      #armv7l-linux = x: x.pkgsCross.armv7l-hf-multiplatform;
      #armv6l-linux = x: x.pkgsCross.raspberryPi;
    };
    pkgs = crosser.${system} (import nixpkgs { system = systemTable.${system}; overlays = [ overlay ]; });
  in {
    packages = {
      inherit (pkgs) extracted eeprom-extractor utils nix initrd_basic;
    };
  });
}
