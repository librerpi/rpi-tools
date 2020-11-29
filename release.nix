let
  self = import ./.;
{
  pi400_keyboard = self.pkgsCross.armv7l-hf-multiplatform.pkgsStatic.keyboard.rootZip;
  bcm2711_msd = pkgsCross.armv7l-hf-multiplatform.pkgsStatic.msd.rootZip;
}
