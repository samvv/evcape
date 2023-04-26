evcape
======

This is a small utility that makes a short press of the control-key act as a
press of the escape-key. It can significantly ease the development workflow
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
sudo apt install libudev-dev
```

Ensure Meson is installed on your system. 

```sh
python3 -m pip install -U --user meson
```

Finally, generate the build rules and compile the sources:

```sh
meson build
ninja -C build
```

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

## Related Work

 - [A Python implementation of evcape](https://github.com/wbolster/evcape/)
 - [Interception Tools](https://gitlab.com/interception/linux/tools)

## License

This code is generously licensed under the MIT license.

