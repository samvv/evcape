
all:
	@echo 'Run `sudo make install` to build and install evcape on this computer.'

build/evcape:
	meson setup build
	ninja -C build evcape

.PHONY: install
install: build/evcape
	sudo install -v -m 644 50-evcape.rules /etc/udev/rules.d/50-evcape.rules
	sudo install -v evcape.service /etc/systemd/system/evcape.service
	sudo install -v evcape.timer /etc/systemd/system/evcape.timer
	sudo install -v build/evcape /usr/local/bin/evcape

.PHONY: uninstall
uninstall:
	sudo rm -f /etc/udev/rules.d/50-evcape.rules
	sudo rm -f /etc/systemd/system/evcape.service
	sudo rm -f /etc/systemd/system/evcape.timer
	sudo rm -f /usr/local/bin/evcape

.PHONY: distclean
distclean:
	rm -rf build

