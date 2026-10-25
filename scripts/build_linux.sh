#!/usr/bin/env bash
# Builds one Linux preset (CMakePresets.json) and runs its tests (rexbox's
# probes through ctest).
#
#   scripts/build_linux.sh [x64|x86] [debug|release] [us|jp] [--no-test] [--target <name>]...
#
# x86 is a -m32 build: it needs gcc-multilib/g++-multilib and the 32-bit
# development libraries SDL3 compiles against. jp builds for the Japanese
# disc into build/linux-<arch>-<config>-jp/, with its own generated code
# (docs/design/20261020-i108-japanese-variant.md). The procedure is in
# docs/guides/linux-setup-and-build.md.
set -euo pipefail

arch="x64"
config="debug"
variant="us"
run_tests=1
targets=()
positional=0
while [ $# -gt 0 ]; do
    case "$1" in
        --no-test) run_tests=0 ;;
        --target)
            [ $# -ge 2 ] || { echo "--target needs a name" >&2; exit 2; }
            targets+=("$2")
            shift
            ;;
        -*) echo "unknown option: $1" >&2; exit 2 ;;
        *)
            case "$positional" in
                0) arch="$1" ;;
                1) config="$1" ;;
                2) variant="$1" ;;
                *) echo "unexpected argument: $1" >&2; exit 2 ;;
            esac
            positional=$((positional + 1))
            ;;
    esac
    shift
done
case "$arch" in x64|x86) ;; *) echo "unknown architecture: $arch (x64 or x86)" >&2; exit 2 ;; esac
case "$config" in debug|release) ;; *) echo "unknown configuration: $config (debug or release)" >&2; exit 2 ;; esac
case "$variant" in us|jp) ;; *) echo "unknown variant: $variant (us or jp)" >&2; exit 2 ;; esac

preset="linux-$arch-$config"
if [ "$variant" = "jp" ]; then
    preset="$preset-jp"
fi
root="$(cd "$(dirname "$0")/.." && pwd)"
cd "$root"
# rexbox carries its own submodules (SDL, imgui, smaa, XbSymbolDatabase), so a
# plain `git submodule update --init` leaves them empty and the configure stops
# inside rexbox. Say the recursive command before CMake runs. A tree without
# git metadata (an exported source) skips the check. A line starting with "-"
# is a registered submodule that is not checked out.
if [ -e .git ] && command -v git >/dev/null 2>&1; then
    missing="$(git submodule status --recursive | awk '/^-/ { print $2 }')"
    if [ -n "$missing" ]; then
        echo "submodule(s) not checked out: $(echo "$missing" | xargs)" >&2
        echo "run: git submodule update --init --recursive" >&2
        exit 2
    fi
fi
cmake --preset "$preset"
if [ ${#targets[@]} -gt 0 ]; then
    cmake --build --preset "$preset" --parallel --target "${targets[@]}"
    run_tests=0  # a partial build may lack the probes
else
    cmake --build --preset "$preset" --parallel
fi
if [ "$run_tests" = 1 ]; then
    ctest --preset "$preset"
fi
