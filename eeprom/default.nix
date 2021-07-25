{ haskellPackages, runCommand, lib }:

let
  myghc = haskellPackages.ghcWithPackages (ps: with ps; [ formatting aeson cryptohash-sha256 base16-bytestring ]);
in runCommand "extractor" {
  buildInputs = [ myghc ];
  src = lib.cleanSource ./.;
} ''
  unpackPhase
  cd $sourceRoot
  mkdir -pv $out/bin
  ghc ./extractor.hs -o $out/bin/extractor -Wall
''
