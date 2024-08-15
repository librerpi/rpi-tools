{ stdenv }:

stdenv.mkDerivation {
  name = "edid";
  src = ./.;
}
