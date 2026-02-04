# ---- Configuration ----
LIB_ROOT ?= $(error LIB_ROOT is not set)
BUILD_DIR := ./build

PROJECT_SRC := src\wasm/libgit2_wasm.c
LIBGIT2_A := $(BUILD_DIR)/libgit2.a
OUTPUT := src/wasm-build/libgit2_wasm.js

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
	-sSTACK_SIZE=524288 \
	-sUSE_PTHREADS \
	-sPTHREAD_POOL_SIZE=4

# ---- Targets ----
.PHONY: all clean configure build

all: run

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
		$(LIBGIT2_A) \
		-I "$(LIB_ROOT)/include" \
		-o $(OUTPUT) \
		$(EMCC_FLAGS)

run: wasm
	npm start