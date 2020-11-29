let
  sources = import nix/sources.nix;
  common = import ./nix/common.nix;
  msd = import ./nix/msd.nix;
  keyboard = import ./nix/keyboard.nix;
  android-auto = import ./nix/android-auto.nix;
  pkgs = import sources.nixpkgs {
    overlays = [ common msd android-auto keyboard ];
  };
  # nix build -L -f ~/apps/rpi/rpi-tools/ pkgsCross.armv7l-hf-multiplatform.pkgsStatic.msd.rootDir
  # nix build -L -f ~/apps/rpi/rpi-tools/ pkgsCross.armv7l-hf-multiplatform.android-auto.rootDir
in pkgs
