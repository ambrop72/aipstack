{ pkgs ? (import <nixpkgs> {}) }:

let
    aipstackSrc = pkgs.lib.cleanSource ./..;
    
    defines = "-DAIPSTACK_CONFIG_ENABLE_ASSERTIONS";
    optFlags = "-O2";
    
    aipstackExampleFunc =
        { stdenv, libuv }:
        stdenv.mkDerivation rec {
            name = "aipstack_example";
            buildInputs = [ libuv ];
            buildCommand = ''
                mkdir -p $out/bin
                cd ${aipstackSrc}
                (
                    set -x
                    c++ -std=c++14 -I src ${defines} ${optFlags} \
                        examples/aipstack_example.cpp \
                        examples/libuv_platform.cpp \
                        examples/libuv_app_helper.cpp \
                        examples/tap_linux/tap_linux.cpp \
                        -luv \
                        -o $out/bin/aipstack_example
                )
            '';
            dontStrip = true;
        };

in
rec {
    aipstackExample = pkgs.callPackage aipstackExampleFunc {
        #stdenv = pkgs.clangStdenv;
    };
}
