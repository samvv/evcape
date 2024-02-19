
all:
	@echo 'Run `sudo make install` to build and install evcape on this computer.'

build/evcape:
	meson setup build
	ninja -C build evcape

.PHONY: install
install: build/evcape
	install -v 50-evcape.rules /etc/udev/rules.d/50-evcape.rules
	install -v evcape.service /etc/systemd/system/evcape.service
	install -v evcape.timer /etc/systemd/system/evcape.timer
	install -v build/evcape /usr/local/bin/evcape

.PHONY: uninstall
uninstall:
	rm -f /etc/udev/rules.d/50-evcape.rules
	rm -f /etc/systemd/system/evcape.service
	rm -f /etc/systemd/system/evcape.timer
	rm -f /usr/local/bin/evcape

.PHONY: distclean
distclean:
	rm -rf build

