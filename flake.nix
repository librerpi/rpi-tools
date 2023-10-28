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
    nixpkgs-be.url = "github:cleverca22/nixpkgs/aarch64-be";
    #nixpkgs.url = "path:/home/clever/apps/rpi/nixpkgs-test";
  };
  outputs = { self, nixpkgs, utils, rpi-eeprom, rpi-open-firmware, firmware, nixpkgs-be }:
  utils.lib.eachSystem [ "x86_64-linux" "armv7l-linux" "armv6l-linux" "aarch64-linux" "aarch64_be-linux" ] (system:
  let
    overlay = self: super: {
      inherit rpi-eeprom firmware;
      eeprom-extractor = self.callPackage ./eeprom {};
      extracted = self.callPackage ./extractor.nix {};
      utils = super.callPackage ./utils {};
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
    packages = (nixpkgs.lib.optionalAttrs (system != "aarch64_be-linux") {
      inherit (pkgs) extracted eeprom-extractor utils nix initrd_basic;
    }) // (nixpkgs.lib.optionalAttrs (system == "aarch64_be-linux") (let
      be = import ./big-endian { inherit nixpkgs-be; };
    in {
      inherit (be) diskImage script;
      cross-bootstrap = (import (nixpkgs-be + "/pkgs/stdenv/linux/make-bootstrap-tools-cross.nix") { system = "x86_64-linux"; }).aarch64_be.dist;
    }));
    hydraJobs = nixpkgs.lib.optionalAttrs (system == "aarch64_be-linux") (let
    in {
      inherit (self.outputs.packages.${system}) diskImage script cross-bootstrap;
    });
  });
}
