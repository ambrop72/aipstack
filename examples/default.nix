{ pkgs ? (import <nixpkgs> {}) }:

let
    aipstackSrc = pkgs.lib.cleanSource ./..;
    
    stdFlags = "-std=c++14";
    defines = "-DAIPSTACK_CONFIG_ENABLE_ASSERTIONS";
    optFlags = "-O2 -fsanitize=address,undefined";

    baseWarnings = [
        "-Wall" "-Wextra" "-Wpedantic"
    ];
    
    optionalWarnings = [
        "-Wshadow" "-Wswitch-default" "-Wreorder" "-Wredundant-decls"
        "-Woverloaded-virtual" "-Wmissing-declarations" "-Wformat=2"
        "-Wdelete-non-virtual-dtor" "-Wformat-signedness" "-Wlogical-op"
        "-Wold-style-cast" "-Wundef" "-Wextra-semi" "-Wnon-virtual-dtor"
        "-Wreserved-id-macro" "-Wcast-align"
    ];

    optionalWarningsClang = [
        "-Wconversion" "-Wno-sign-conversion"
    ];

    filterSupportedWarnings =
        { stdenv, warnings, compilerMatch ? "" }:
        stdenv.mkDerivation rec {
            name = "aipstack_supported_warnings.txt";
            buildCommand = ''
                (
                    ${if compilerMatch == "" then "" else ''
                        if ! c++ --version | head -n 1 | grep -i "${compilerMatch}" >/dev/null; then
                            return
                        fi
                    ''}
                    touch test.cpp
                    ${stdenv.lib.concatMapStrings (warning: ''
                        if c++ ${stdFlags} -Werror "${warning}" -c test.cpp \
                                >/dev/null 2>&1; then
                            echo -n "${warning} "
                        fi
                    '') warnings}
                ) >"$out"
            '';
        };
    
    supportedOptionalWarnings = stdenv: filterSupportedWarnings {
        inherit stdenv;
        warnings = optionalWarnings;
    };

    supportedOptionalWarningsClang = stdenv: filterSupportedWarnings {
        inherit stdenv;
        warnings = optionalWarningsClang;
        compilerMatch = "clang";
    };
    
    aipstackExampleFunc =
        { stdenv }:
        stdenv.mkDerivation rec {
            name = "aipstack_example";
            buildCommand = ''
                mkdir -p $out/bin
                cd ${aipstackSrc}
                (
                    set -x
                    c++ ${stdFlags} -I src ${defines} -pthread ${optFlags} \
                        ${stdenv.lib.concatStringsSep " " baseWarnings} \
                        $(cat ${supportedOptionalWarnings stdenv}) \
                        $(cat ${supportedOptionalWarningsClang stdenv}) \
                        examples/aipstack_example.cpp \
                        src/aipstack/event_loop/EventLoopAmalgamation.cpp \
                        src/aipstack/tap/TapDeviceAmalgamation.cpp \
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
