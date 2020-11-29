let
  self = import ./.;
in {
  pi400_keyboard = self.pkgsCross.armv7l-hf-multiplatform.pkgsStatic.keyboard.rootZip;
  bcm2711_msd = self.pkgsCross.armv7l-hf-multiplatform.pkgsStatic.msd.rootZip;
}
