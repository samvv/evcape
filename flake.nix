{
  description = "A flake for building evcape";

  inputs = {
    nixpkgs.url = "github:NixOS/nixpkgs/nixos-24.05";
    flake-utils.url = "github:numtide/flake-utils";
  };

  # See https://wiki.nixos.org/wiki/Flakes#Output_schema for more information
  outputs = { self, nixpkgs, flake-utils }: {

    nixosModules.default = let fn = import ./nixos.nix; in fn self;

  } // flake-utils.lib.eachDefaultSystem(system:

    let pkgs = nixpkgs.legacyPackages.${system}; in
    {

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
          longDescription = ''
          evcape allows you to give your Ctrl-key a second life by making it
          function like an Escape-key. The service runs in the background and
          monitors what you type, injecting Escape whenever it is needed.
          '';
        };
      };

      packages.default = self.packages.${system}.evcape;

  });

}
