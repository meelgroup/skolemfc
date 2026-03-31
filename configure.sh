#!/usr/bin/env bash
set -e

# --------------------------------------------------------------------------
# Defaults
# --------------------------------------------------------------------------
AUTO_DOWNLOAD=OFF
BUILD_DIR="build"
BUILD_TYPE="RelWithDebInfo"
STATIC=OFF
EXTRA_ARGS=()

# --------------------------------------------------------------------------
# Argument parsing
# --------------------------------------------------------------------------
while [[ $# -gt 0 ]]; do
    case "$1" in
        -D|--auto-download)
            AUTO_DOWNLOAD=ON
            shift
            ;;
        --build-dir)
            BUILD_DIR="$2"
            shift 2
            ;;
        --build-type)
            BUILD_TYPE="$2"
            shift 2
            ;;
        --static)
            STATIC=ON
            shift
            ;;
        -j)
            # Just consume — user can pass -j to make separately
            shift 2
            ;;
        -h|--help)
            sed -n '/^# configure.sh/,/^[^#]/{ /^[^#]/q; s/^# \{0,1\}//p }' "$0"
            exit 0
            ;;
        --)
            shift
            EXTRA_ARGS=("$@")
            break
            ;;
        *)
            echo "Unknown option: $1" >&2
            echo "Run '$0 --help' for usage." >&2
            exit 1
            ;;
    esac
done

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

# --------------------------------------------------------------------------
# Check submodule state when not auto-downloading
# --------------------------------------------------------------------------
if [[ "$AUTO_DOWNLOAD" == "OFF" ]]; then
    if [[ ! -f "$SCRIPT_DIR/deps/louvain-community/CMakeLists.txt" ]]; then
        echo ""
        echo "ERROR: Local dependency submodules are not initialised."
        echo ""
        echo "Either:"
        echo "  1. Initialise submodules and retry:"
        echo "       git submodule update --init"
        echo "       $0"
        echo ""
        echo "  2. Or let the build system clone all dependencies automatically:"
        echo "       $0 -D"
        echo ""
        exit 1
    fi
    echo "Using local deps/ submodules."
else
    echo "AUTO_DOWNLOAD=ON — dependencies will be cloned into ${BUILD_DIR}/deps/"
fi

# --------------------------------------------------------------------------
# Create build directory and run cmake
# --------------------------------------------------------------------------
mkdir -p "$SCRIPT_DIR/$BUILD_DIR"
cd "$SCRIPT_DIR/$BUILD_DIR"

cmake_args=(
    -DCMAKE_BUILD_TYPE="$BUILD_TYPE"
    -DAUTO_DOWNLOAD="$AUTO_DOWNLOAD"
)

if [[ "$STATIC" == "ON" ]]; then
    cmake_args+=(-DSTATICCOMPILE=ON)
fi

cmake_args+=("${EXTRA_ARGS[@]}")
cmake_args+=("$SCRIPT_DIR")

echo ""
echo "Running: cmake ${cmake_args[*]}"
echo ""
cmake "${cmake_args[@]}"

echo ""
echo "Configuration complete.  To build:"
echo "  cd $BUILD_DIR && make -j\$(nproc)"