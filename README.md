# EMP-ZK WASM VOLE Benchmark

Fork of [emp-toolkit/emp-zk](https://github.com/emp-toolkit/emp-zk) for measuring VOLE receiver performance in the browser.

Uses BLAKE3 hashing instead of AES-NI to enable WebAssembly compatibility. The WASM receiver connects to a native sender via WebSocket proxy.

## Build

```bash
./build.sh
```

This will check for dependencies, install Emscripten if needed, and build both the native sender and WASM receiver.

## Run

```bash
cd wasm
./run_test.sh
```

Then open http://localhost:8000 in your browser and click "Run VOLE".

## Expected Output

```
========================================
Results
========================================
Total time:      5542 ms
VOLEs generated: 10005354
Rate: 1.81 million VOLEs/sec
========================================

Network: sent=72691 bytes, recv=2389052 bytes

--- Mock Statistics ---
Base COTs:   72615
Base VOLEs:  1821
```
