# ---- Configuration ----
LIB_ROOT ?= $(error LIB_ROOT is not set)
OPT ?= O0

BUILD_DIR := ./lib-wasm-build
NATIVE_BUILD_DIR := ./lib-native-build

NATIVE_SRC := src/c/libgit2_core.c src/c/libgit2_native.c
PROJECT_SRC := src/c/libgit2_wasm.c src/c/libgit2_core.c

WASM_LIBGIT2_A := $(BUILD_DIR)/libgit2.a
NATIVE_LIBGIT2_A := $(NATIVE_BUILD_DIR)/libgit2.a


WASM_OUTPUT := src/wasm-build/libgit2_wasm.js
NATIVE_OUTPUT := src/native-build/libgit2_native

CMAKE_FLAGS := \
	-DBUILD_SHARED_LIBS=OFF \
	-DUSE_HTTPS=OFF \
	-DUSE_HTTP=OFF \
	-DUSE_AUTH_NTLM=OFF \
	-DCMAKE_C_FLAGS="-pthread" \
	-DCMAKE_CXX_FLAGS="-pthread" \
	-DBUILD_CLI=OFF \

EMCC_FLAGS := \
	-sWASMFS=1 \
	-sEXPORTED_FUNCTIONS='["_init","_shutdown","_free","_mount_opfs_in_thread","_is_opfs_ready","_open_repo","_walk_commits","_get_commit_info","_get_commit_diff","_commit_count","_test_write"]' \
	-sEXPORTED_RUNTIME_METHODS='["ccall","cwrap","UTF8ToString"]' \
	-sMODULARIZE \
	-sEXPORT_ES6 \
	-sALLOW_MEMORY_GROWTH \
	-sSTACK_SIZE=524288 \
	-sUSE_PTHREADS \
	-sPTHREAD_POOL_SIZE=4

# ---- Targets ----
.PHONY: all npm-install clean configure build wasm run configure-native build-native

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
		-$(OPT) \
		$(EMCC_FLAGS)

$(NATIVE_BUILD_DIR):
	mkdir "$(NATIVE_BUILD_DIR)"

configure-native: $(NATIVE_BUILD_DIR)
	cd "$(NATIVE_BUILD_DIR)" && cmake "$(LIB_ROOT)" \
		-G "MinGW Makefiles" \
		-DBUILD_SHARED_LIBS=OFF \
		-DUSE_HTTPS=OFF \
		-DUSE_SSH=OFF \
		-DUSE_AUTH_NTLM=OFF \
		-DBUILD_CLI=OFF \

build-native: configure-native
	cd "$(NATIVE_BUILD_DIR)" && make VERBOSE=1

native: build-native
	gcc -Wall $(NATIVE_SRC) \
		$(NATIVE_LIBGIT2_A) \
		-I "$(LIB_ROOT)/include" \
		-$(OPT) \
		-o $(NATIVE_OUTPUT) \
		-lsecur32 \
		-lws2_32 \
		-Wl,--allow-multiple-definition


run: wasm
	npm start