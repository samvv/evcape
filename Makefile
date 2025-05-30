
VERSION=1.0.0

all: build/evcape
	sudo --preserve-env=EVCAPE_LOG_LEVEL ./build/evcape

build/build.ninja: meson.build
	meson setup build

build/evcape: evcape.c build/build.ninja
	ninja -C build evcape

.PHONY: pack
pack: build/evcape
	install -Dm644 evcape.service pkg/etc/systemd/system
	install -Dm0755 build/evcape pkg/usr/bin/
	tar -cvzf evcape-$(VERSION).tar.gz pkg/*

.PHONY: install
install: build/evcape
	sudo install -v evcape.service /etc/systemd/system/evcape.service
	sudo install -v build/evcape /usr/local/bin/evcape

.PHONY: uninstall
uninstall:
	sudo rm -f /etc/systemd/system/evcape.service
	sudo rm -f /usr/local/bin/evcape

.PHONY: clean
clean:
	ninja -C build clean

.PHONY: distclean
distclean:
	rm -rf build

