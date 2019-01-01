{ stdenv, lib, enableAssertions, enableSanitizers, customFlags }:
let
    aipstackSrc = lib.cleanSource ../.;

    stdFlags = [ "-std=c++17" ];

    defines = lib.optional enableAssertions "-DAIPSTACK_CONFIG_ENABLE_ASSERTIONS";

    sanitizeFlags = lib.optional enableSanitizers "-fsanitize=address,undefined";

    baseWarnings = [ "-Wall" "-Wextra" "-Wpedantic" ];
    
    optionalWarnings = [
        "-Wshadow" "-Wswitch-default" "-Wreorder" "-Wredundant-decls"
        "-Woverloaded-virtual" "-Wmissing-declarations" "-Wformat=2"
        "-Wdelete-non-virtual-dtor" "-Wformat-signedness" "-Wlogical-op"
        "-Wold-style-cast" "-Wundef" "-Wextra-semi" "-Wreserved-id-macro"
        "-Wcast-align" "-Wno-tautological-constant-out-of-range-compare"
    ];

    optionalWarningsClang = [ "-Wconversion" "-Wnon-virtual-dtor" ];

    filterSupportedWarnings =
        { warnings, compilerMatch ? "" }:
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
                    ${lib.concatMapStrings (warning: ''
                        if c++ ${concatFlags stdFlags} -Werror "${warning}" -c test.cpp \
                                >/dev/null 2>&1; then
                            echo -n "${warning} "
                        fi
                    '') warnings}
                ) >"$out"
            '';
        };
    
    supportedOptionalWarnings = filterSupportedWarnings {
        warnings = optionalWarnings;
    };

    supportedOptionalWarningsClang = filterSupportedWarnings {
        warnings = optionalWarningsClang;
        compilerMatch = "clang";
    };

    concatFlags = flags: lib.concatStringsSep " " flags;

in stdenv.mkDerivation {
    name = "aipstack_example";

    buildCommand = ''
        mkdir -p $out/bin
        cd ${aipstackSrc}
        (
            set -x
            c++ \
                examples/aipstack_example.cpp \
                src/aipstack/event_loop/EventLoopAmalgamation.cpp \
                src/aipstack/tap/TapDeviceAmalgamation.cpp \
                -o $out/bin/aipstack_example \
                -I src -pthread \
                ${concatFlags stdFlags} ${concatFlags sanitizeFlags} \
                ${concatFlags baseWarnings} \
                $(cat ${supportedOptionalWarnings}) \
                $(cat ${supportedOptionalWarningsClang}) \
                ${concatFlags defines} ${concatFlags customFlags}
        )
    '';

    dontStrip = true;
}
