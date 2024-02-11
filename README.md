evcape
======

This is a small utility that makes a short press of the Control-key act as a
press of the Escape-key. It can significantly ease the development workflow
when using a text editor such as Vim. Because it is written in C, this program
is also very performant with minimal memory overhead.

## Installation

### Debian/Ubuntu

An APT repository will soon be avaialbe so that the latest version can be
easily installed.

### Building From Source

Make sure you have the right packages installed. On Debian/Ubuntu, this will
look something like this:

```sh
sudo apt install libudev-dev libevdev-dev
```

Ensure Meson is installed on your system. 

```sh
python3 -m pip install -U --user meson
```

To install the package, simply run the following command:

```sh
sudo make install
```

This will install the following files:

 - `/etc/udev/rules.d/50-evcape.rules`
 - `/etc/systemd/system/evcape.service`
 - `/usr/local/bin/evcape`

You might want to run the following commands to activate `evcape` immediately:

```
sudo udevadm control --reload-rules && sudo udevadm trigger
sudo systemctl daemon-reload
sudo systemctl enable evcape
sudo systemctl start evcape
```

If you just want the binary, run the following commands:

```sh
meson setup build
ninja -C build evcape
```

The binary will be available as `build/evcape`.

### Setting Up CapsLock

For GNOME, use the following command to copy Ctrl to CapsLock.

:warning: This command overrides existing configuration!

```bash
gsettings set org.gnome.desktop.input-sources xkb-options "['ctrl:nocaps']"
```

Alternatively, the following command might work when working on Xwayland or X11:

```bash
setxkbmap -option "ctrl:nocaps"
```

### Running

Simply run `evcape` with sudo privileges and you're good to go!

evcape needs sudo privileges because it intercepts and then injects raw key
events from/to the keyboard subsystem. Because Wayland does not offer a
standard protocol to do the same, the driver is where we need to be.

The following environment variables can be set to influence the behavior of evcape:

| Name               |                                                      |
|--------------------|------------------------------------------------------|
| `EVCAPE_LOG_LEVEL` | An integer from 0 (no logging) to 6 (log everything) |

### systemd

The following is an example of how a systemd file could look. Adjust it to your
needs and save it to `/etc/systemd/system/evcape.service`.

```systemd
[Unit]
Description=Make the control-key act as an escape-key

[Service]
Type=simple
ExecStart=/usr/local/bin/evcape

[Install]
WantedBy=multi-user.target
```

## Related Work

 - [A Python implementation of evcape](https://github.com/wbolster/evcape/)
 - [Interception Tools](https://gitlab.com/interception/linux/tools) and in particular [caps2esc](https://gitlab.com/interception/linux/plugins/caps2esc)

## License

This code is generously licensed under the MIT license.

