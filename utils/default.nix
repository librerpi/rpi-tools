{ stdenv, common, libraspberrypi, libdrm, pkgconfig, lib }:

stdenv.mkDerivation {
  name = "utils";
  nativeBuildInputs = [ pkgconfig ];
  buildInputs = [
    common libraspberrypi libdrm
    "${libraspberrypi.src}/host_support"
  ];
  src = lib.cleanSource ./.;
  dontStrip = true;
  enableParallelBuilding = true;
}
