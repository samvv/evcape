
all: build/evcape
	sudo --preserve-env=EVCAPE_LOG_LEVEL ./build/evcape

build/build.ninja: meson.build
	meson setup build

build/evcape: evcape.c build/build.ninja
	ninja -C build evcape

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

