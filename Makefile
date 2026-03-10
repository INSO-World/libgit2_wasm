# ---- Configuration ----
LIB_ROOT ?= $(error LIB_ROOT is not set)
BUILD_DIR := ./build

NATIVE_SRC := src/c/libgit2_core.c src/c/libgit2_native.c
PROJECT_SRC := src/c/libgit2_wasm.c src/c/libgit2_core.c

WASM_LIBGIT2_A := $(BUILD_DIR)/libgit2.a
NATIVE_LIBGIT2_A := native-build/libgit2.dll.a

WASM_OUTPUT := src/wasm-build/libgit2_wasm.js
NATIVE_OUTPUT := src/native-build/libgit2_native

CMAKE_FLAGS := \
	-DUSE_HTTPS=OFF \
	-DUSE_HTTP=OFF \
	-DUSE_AUTH_NTLM=OFF \
	-DCMAKE_C_FLAGS="-pthread" \
	-DCMAKE_CXX_FLAGS="-pthread"

EMCC_FLAGS := \
	-sWASMFS=1 \
	-sEXPORTED_FUNCTIONS='["_init","_free","_mount_opfs","_open_repo","_shutdown","_git_libgit2_init","_git_libgit2_shutdown"]' \
	-sEXPORTED_RUNTIME_METHODS='["ccall","cwrap","UTF8ToString"]' \
	-sMODULARIZE \
	-sEXPORT_ES6 \
	-sALLOW_MEMORY_GROWTH \
	-sSTACK_SIZE=524288 \
	-sUSE_PTHREADS \
	-sPTHREAD_POOL_SIZE=4

# ---- Targets ----
.PHONY: all npm-install clean configure build wasm run

all: npm-install run

npm-install:
	npm install

clean:
	rmdir /s /q "$(BUILD_DIR)" 2>nul || exit 0

$(BUILD_DIR):
	mkdir "$(BUILD_DIR)"

configure: $(BUILD_DIR)
	cd "$(BUILD_DIR)" && emcmake cmake "$(LIB_ROOT)" $(CMAKE_FLAGS)

build: configure
	cd "$(BUILD_DIR)" && emmake make VERBOSE=1

wasm: build
	emcc $(PROJECT_SRC) \
		$(WASM_LIBGIT2_A) \
		-I "$(LIB_ROOT)/include" \
		-o $(WASM_OUTPUT) \
		$(EMCC_FLAGS)

native: 
	gcc -Wall $(NATIVE_SRC) \
		$(NATIVE_LIBGIT2_A) \
		-I "$(LIB_ROOT)/include" \
		-o $(NATIVE_OUTPUT)

run: wasm
	npm start