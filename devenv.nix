{ pkgs, lib, config, inputs, ... }:

{

  packages = [
    pkgs.git
    pkgs.meson
    pkgs.gcc
    pkgs.gnumake
    pkgs.ninja
    pkgs.libevdev
    pkgs.pkg-config
    pkgs.systemd
  ];

  enterTest = ''
    echo "Running tests"
  '';

}
