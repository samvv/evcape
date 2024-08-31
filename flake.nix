{
  description = "A flake for building evcape";

  inputs = {
    nixpkgs.url = "github:NixOS/nixpkgs/nixos-24.05";
    flake-utils.url = "github:numtide/flake-utils";
  };

  outputs = { self, nixpkgs, flake-utils }:

    flake-utils.lib.eachDefaultSystem (system:
      let
        pkgs = import nixpkgs { inherit system; };
      in {
        packages.evcape = pkgs.stdenv.mkDerivation {
          name = "evcape";
          src = self;
          nativeBuildInputs = with pkgs; [
            meson
            ninja
            pkg-config
          ];
          buildInputs = with pkgs; [
            libevdev
            systemd
          ];
          meta = with pkgs.lib; {
            homepage = "https://github.com/samvv/evcape";
            description = "A small tool for making the Control key act as an Escape-key on Linux/Wayland";
            license = licenses.mit;
            maintainers = [ maintainers.samvv ];
            platforms = platforms.linux;
            mainProgram = "evcape";
          };
      };
      packages.default = self.packages.${system}.evcape;
    }
  );

}
