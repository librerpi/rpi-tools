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
  };
  outputs = { self, nixpkgs, utils, rpi-eeprom, rpi-open-firmware }:
  utils.lib.eachSystem [ "x86_64-linux" "armv7l-linux" "armv6l-linux" ] (system:
  let
    overlay = self: super: {
      inherit rpi-eeprom;
      eeprom-extractor = self.callPackage ./eeprom {};
      extracted = self.callPackage ./extractor.nix {};
      utils = self.callPackage ./utils {};
      common = self.callPackage "${rpi-open-firmware}/common" {};
      tlsf = null;
    };
    systemTable = {
      x86_64-linux = "x86_64-linux";
      armv7l-linux = "x86_64-linux";
      armv6l-linux = "x86_64-linux";
    };
    crosser = {
      x86_64-linux = x: x;
      armv7l-linux = x: x.pkgsCross.armv7l-hf-multiplatform;
      armv6l-linux = x: x.pkgsCross.raspberryPi;
    };
    pkgs = crosser.${system} (import nixpkgs { system = systemTable.${system}; overlays = [ overlay ]; });
  in {
    packages = {
      inherit (pkgs) extracted eeprom-extractor utils nix;
    };
  });
}
