
VERSION=1.0.1

all: build/evcape

build/build.ninja: meson.build
	meson setup build

build/evcape: evcape.c build/build.ninja
	ninja -C build evcape

.PHONY: test
test:
	sudo --preserve-env=EVCAPE_LOG_LEVEL ./build/evcape

.PHONY: pkg
pkg: build/evcape
	install -Dm644 evcape.service -t pkg/usr/lib/systemd/system/
	install -Dm0755 build/evcape -t pkg/usr/bin/
	cd pkg && tar -cvzf ../evcape-$(VERSION).tar.gz *

.PHONY: install
install: build/evcape
	sudo install -Dm644 -v evcape.service $(DESTDIR)/usr/lib/systemd/system/evcape.service
	sudo install -Dm755 -v build/evcape $(DESTDIR)/usr/bin/evcape

.PHONY: uninstall
uninstall:
	sudo rm -f $(DESTDIR)/etc/systemd/system/evcape.service
	sudo rm -f $(DESTDIR)/usr/bin/evcape

.PHONY: clean
clean:
	ninja -C build clean

.PHONY: distclean
distclean:
	rm -rf build

