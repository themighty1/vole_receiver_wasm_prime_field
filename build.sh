#!/bin/bash
# Build native sender and WASM receiver

set -e

cd "$(dirname "$0")"

echo "=== Building VOLE Benchmark ==="
echo ""

# Check dependencies
MISSING=""

if ! command -v cmake &> /dev/null; then
    MISSING="$MISSING cmake"
fi

if ! command -v make &> /dev/null; then
    MISSING="$MISSING make"
fi

if ! command -v g++ &> /dev/null; then
    MISSING="$MISSING g++"
fi

if ! command -v node &> /dev/null; then
    MISSING="$MISSING nodejs"
fi

if ! command -v npm &> /dev/null; then
    MISSING="$MISSING npm"
fi

if [ -n "$MISSING" ]; then
    echo "Missing dependencies:$MISSING"
    echo ""
    echo "Install with:"
    echo "  Ubuntu/Debian: sudo apt install$MISSING"
    echo "  Fedora: sudo dnf install$MISSING"
    exit 1
fi

# Check/install Emscripten
find_emscripten() {
    # Already in PATH?
    if command -v emcmake &> /dev/null; then
        return 0
    fi

    # Check common emsdk locations and source if found
    for emsdk_path in "$HOME/emsdk" "./emsdk" "/opt/emsdk" "$EMSDK"; do
        if [ -f "$emsdk_path/emsdk_env.sh" ]; then
            echo "Found emsdk at $emsdk_path, activating..."
            source "$emsdk_path/emsdk_env.sh"
            return 0
        fi
    done

    # Check if emcc exists (system package install)
    if command -v emcc &> /dev/null; then
        EMCC_DIR=$(dirname "$(which emcc)")

        # Fedora/system install: emcmake might be in same dir or nearby
        if [ -f "$EMCC_DIR/emcmake" ]; then
            export PATH="$EMCC_DIR:$PATH"
            return 0
        fi

        # Try common system locations
        for cmake_wrapper in /usr/lib/emscripten/emcmake /usr/share/emscripten/emcmake; do
            if [ -f "$cmake_wrapper" ]; then
                export PATH="$(dirname $cmake_wrapper):$PATH"
                return 0
            fi
        done

        # emcc exists but no emcmake - can use emcc directly with cmake
        echo "Found emcc but not emcmake. Using emcc directly..."
        return 0
    fi

    return 1
}

if ! find_emscripten; then
    echo "Emscripten not found. Installing..."
    echo ""

    git clone https://github.com/emscripten-core/emsdk.git
    cd emsdk
    ./emsdk install latest
    ./emsdk activate latest
    source ./emsdk_env.sh
    cd ..

    echo ""
    echo "Emscripten installed. Add to your shell profile:"
    echo "  source $(pwd)/emsdk/emsdk_env.sh"
    echo ""
fi

# Verify emcmake works
if ! command -v emcmake &> /dev/null; then
    echo "Error: emcmake not found after setup"
    echo ""
    echo "If you installed Emscripten via package manager, you may need to:"
    echo "  Fedora: sudo dnf install emscripten"
    echo "  Ubuntu: sudo apt install emscripten"
    echo ""
    echo "Or install emsdk manually:"
    echo "  git clone https://github.com/emscripten-core/emsdk.git"
    echo "  cd emsdk && ./emsdk install latest && ./emsdk activate latest"
    echo "  source emsdk_env.sh"
    exit 1
fi

# Build native sender
echo "1. Building native sender..."
mkdir -p native/build
cd native/build
cmake ..
make -j$(nproc)
cd ../..
echo "   Done: native/build/vole_sender"
echo ""

# Build WASM receiver
echo "2. Building WASM receiver..."
mkdir -p wasm/build
cd wasm/build
emcmake cmake ..
emmake make -j$(nproc)
cd ../..
echo "   Done: wasm/build/vole_receiver.js"
echo ""

# Install WebSocket proxy dependency
echo "3. Installing WebSocket proxy dependency..."
cd wasm
npm install
cd ..
echo "   Done"
echo ""

echo "=== Build complete ==="
echo ""
echo "Run: cd wasm && ./run_test.sh"
echo "Then open: http://localhost:8000/vole_receiver.html"
