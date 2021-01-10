{ stdenv, common, libraspberrypi, libdrm, pkgconfig }:

stdenv.mkDerivation {
  name = "utils";
  nativeBuildInputs = [ pkgconfig ];
  buildInputs = [
    common libraspberrypi libdrm
    "${libraspberrypi.src}/host_support"
  ];
  src = stdenv.lib.cleanSource ./.;
  dontStrip = true;
  enableParallelBuilding = true;
}
