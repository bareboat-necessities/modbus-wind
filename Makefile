# Convenience targets for local PC builds and OpenWrt SDK packaging.

BUILD_DIR ?= build
CMAKE_BUILD_TYPE ?= Release
OPENWRT_VERSION ?= 24.10.6
OPENWRT_TARGET ?= x86/64
OPENWRT_JOBS ?= $(shell nproc 2>/dev/null || sysctl -n hw.ncpu 2>/dev/null || echo 2)
OPENWRT_WORK_DIR ?= .openwrt-work

.PHONY: all build configure clean install package-openwrt

all: build

configure:
	cmake -S . -B $(BUILD_DIR) -DCMAKE_BUILD_TYPE=$(CMAKE_BUILD_TYPE)

build: configure
	cmake --build $(BUILD_DIR) --config $(CMAKE_BUILD_TYPE) --parallel

install: build
	cmake --install $(BUILD_DIR) --config $(CMAKE_BUILD_TYPE)

clean:
	cmake -E rm -rf $(BUILD_DIR) dist $(OPENWRT_WORK_DIR)

package-openwrt:
	OPENWRT_VERSION=$(OPENWRT_VERSION) \
	OPENWRT_TARGET=$(OPENWRT_TARGET) \
	OPENWRT_JOBS=$(OPENWRT_JOBS) \
	OPENWRT_WORK_DIR=$(OPENWRT_WORK_DIR) \
		./scripts/openwrt-build-package.sh
