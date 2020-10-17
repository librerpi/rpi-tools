{ stdenv, common, raspberrypi-tools, libdrm, pkgconfig }:

stdenv.mkDerivation {
  name = "utils";
  nativeBuildInputs = [ pkgconfig ];
  buildInputs = [
    common raspberrypi-tools libdrm
    "${raspberrypi-tools.src}/host_support"
  ];
  src = stdenv.lib.cleanSource ./.;
  dontStrip = true;
  enableParallelBuilding = true;
}
