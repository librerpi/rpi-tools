{ stdenv, openssl, nukeReferences, protobuf }:

stdenv.mkDerivation {
  name = "android-auto";
  src = ./.;
  nativeBuildInputs = [ nukeReferences protobuf ];
  buildInputs = [ openssl protobuf ];
  postFixup = ''
    rm $out/nix-support/propagated-build-inputs || true
  '';
  enableParallelBuilding = true;
}
