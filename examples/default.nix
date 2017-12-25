{ pkgs ? (import <nixpkgs> {}) }:

let
    aipstackSrc = pkgs.lib.cleanSource ./..;
    
    stdFlags = "-std=c++14";
    defines = "-DAIPSTACK_CONFIG_ENABLE_ASSERTIONS";
    optFlags = "-O2";

    baseWarnings = [
        "-Wall" "-Wextra" "-Wpedantic"
        "-Wno-missing-field-initializers"
    ];
    
    optionalWarnings = [
        "-Wshadow" "-Wswitch-default" "-Wreorder" "-Wredundant-decls"
        "-Woverloaded-virtual" "-Wmissing-declarations" "-Wformat=2"
        "-Wdelete-non-virtual-dtor" "-Wformat-signedness" "-Wlogical-op"
    ];

    filterSupportedWarnings =
        { stdenv }:
        stdenv.mkDerivation rec {
            name = "aipstack_supported_warnings.txt";
            buildCommand = ''
                touch test.cpp
                (
                    ${stdenv.lib.concatMapStrings (warning: ''
                        if c++ ${stdFlags} -Werror "${warning}" -c test.cpp \
                                >/dev/null 2>&1; then
                            echo -n "${warning} "
                        fi
                    '') optionalWarnings}
                ) >"$out"
            '';
        };
    
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
                    c++ ${stdFlags} -I src ${defines} ${optFlags} \
                        ${stdenv.lib.concatStringsSep " " baseWarnings} \
                        $(cat ${filterSupportedWarnings {inherit stdenv;}}) \
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
