with import <nixpkgs> {};

rec {
  elf-tool = stdenv.mkDerivation {
    name = "elf-tool";
    src = ./.;
  };
  convert = { rom }: runCommandCC "convert" {nativeBuildInputs = [ elf-tool file util-linux ]; } ''
    mkdir $out/
    elf-tool -i ${rom} -o $out/out.elf
    ls -lh $out/
    file $out/out.elf
    readelf -hl $out/out.elf
  '';
}
