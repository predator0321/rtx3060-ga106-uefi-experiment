# SPDX-License-Identifier: GPL-2.0-only
PYTHON ?= python3
INPUTS ?= $(abspath audit-inputs)
KDIR ?= /lib/modules/$(shell uname -r)/build
LLVM ?= 1
MODULEDIR := kernel/gpu_unlock
ASSETS := $(abspath build/firmware)

.PHONY: all prepare module test preflight stage clean
all: module test

prepare:
	$(PYTHON) tools/prepare.py --inputs "$(INPUTS)"

module:
	$(MAKE) -C $(MODULEDIR) KDIR="$(KDIR)" LLVM="$(LLVM)"

test: prepare
	GPU_UNLOCK_ASSETS="$(ASSETS)" $(PYTHON) -m unittest discover -s tests -v

preflight:
	$(PYTHON) tools/preflight.py

# Build a reviewable staging directory only; never install into the host.
stage: prepare module
	mkdir -p build/stage/usr/lib/firmware/ga106-unlock/610.43.03
	install -m644 $(ASSETS)/*.bin build/stage/usr/lib/firmware/ga106-unlock/610.43.03/
	install -Dm644 $(MODULEDIR)/ga106_unlock.ko build/stage/usr/lib/modules/$(shell uname -r)/extra/ga106_unlock.ko
	install -Dm644 $(ASSETS)/NVIDIA-LICENSE build/stage/usr/share/licenses/ga106-unlock/NVIDIA-LICENSE
	install -m644 licenses/*.txt firmware/NOTICE.md build/stage/usr/share/licenses/ga106-unlock/

clean:
	$(MAKE) -C $(MODULEDIR) KDIR="$(KDIR)" LLVM="$(LLVM)" clean
