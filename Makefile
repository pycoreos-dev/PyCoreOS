.PHONY: build iso run test beta release clean install-deps

PYTHON ?= python3

build:
	$(PYTHON) scripts/build.py build

iso:
	$(PYTHON) scripts/build.py iso

run:
	$(PYTHON) scripts/build.py run

test:
	$(PYTHON) scripts/build.py test

beta:
	$(PYTHON) scripts/release.py beta

release: beta

clean:
	$(PYTHON) scripts/build.py clean

install-deps:
	sudo apt-get update
	sudo apt-get install -y python3 make grub-common xorriso qemu-system-x86 gcc-multilib g++-multilib
