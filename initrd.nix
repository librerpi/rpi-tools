{ pkgs, lib }:

let
  eval = lib.evalModules {
    prefix = [];
    check = true;
    modules = [ ./initrd/core.nix ./initrd/pi400-keyboard.nix ];
    args = {
      initialPkgs = pkgs;
    };
  };
in eval.config.results.rootDir // { inherit eval; }
