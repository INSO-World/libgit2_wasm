# libgit2_wasm

This project was developed as part of a bachelor's thesis and explores the
practical limitations of compiling native C libraries to WebAssembly (WASM).
As a case study, the Git library **libgit2** is compiled to WASM using
**Emscripten** and executed in the browser.

The project demonstrates:
- compiling a non-trivial C library to WebAssembly
- interacting with the library from JavaScript
- using OPFS (Origin Private File System)
- running WebAssembly with pthreads in a browser environment

---

## Requirements

- **Emscripten SDK** (emsdk) installed and activated
- **CMake**
- **Make**
- **Node.js** (for the local development server)
- A modern browser supporting:
  - WebAssembly threads
  - SharedArrayBuffer
  - OPFS

> The project requires cross-origin isolation (COOP/COEP) and therefore
> must be accessed through the provided server.

---

## Build and Run

Building and running the project consists of two steps.

### 1. Set the libgit2 source path

The path to the libgit2 source directory must be provided via an environment
variable. This directory must contain `CMakeLists.txt`.

**PowerShell (Windows):**
```powershell
$env:LIB_ROOT="C:\path\to\libgit2"
```

### 2. Build and start the project

**PowerShell (Windows):**
```powershell
$env:LIB_ROOT="C:\path\to\libgit2"
```
1. Build libgit2 for WebAssembly using Emscripten

2. Link the library against the project’s C/WASM interface

3. Compile the final WebAssembly and JavaScript glue code

4. Start a local Express server at http://localhost:8080


## Project structure
```text
project-root/
│
├─ src/
│  ├─ wasm/
│  │  └─ libgit2_wasm.c        # C ↔ WASM interface layer
│  │
│  ├─ js/
│  │  │
│  │  ├─ opfs/                 # OPFS file handling
│  │  │  └─ opfsStorage.js
│  │  │
│  │  ├─ benchmarkWorker.js    # Worker thread used for benchmarking
│  │  │ 
│  │  ├─ config.js             # configuration for main.js
│  │  │ 
│  │  └─ main.js               # Application entry point
│  │
│  └─ index.html
│
├─ server/
│  └─ server.js                # Express server (COOP/COEP enabled)
│
├─ Makefile
├─ package.json
└─ README.md

```
