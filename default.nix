{ pkgs ? (import <nixpkgs> {}) }:
rec {
    aipstackExample = pkgs.callPackage examples/aipstack_example.nix {
        #stdenv = pkgs.clangStdenv;
        enableAssertions = true;
        enableSanitizers = true;
        customFlags = [ "-O2" ];
    };
}
