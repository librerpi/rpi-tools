with import <nixpkgs> {};

let
  myghc = haskellPackages.ghcWithPackages (ps: with ps; [ formatting ]);
in runCommand "extractor" { buildInputs = [ myghc ]; } ''
  mkdir -pv $out/bin
  ghc ${./extractor.hs} -o $out/bin/extractor
''
