{
  description = "gulp flake";

  inputs.nixpkgs.url = "github:nixos/nixpkgs";
  inputs.treefmt-nix.url = "github:numtide/treefmt-nix";
  inputs.treefmt-nix.inputs.nixpkgs.follows = "nixpkgs";

  outputs =
    { self
    , nixpkgs
    , treefmt-nix
    , ...
    }:
    let
      eachSystem = f:
        nixpkgs.lib.genAttrs [
          "aarch64-darwin"
          "aarch64-linux"
          "x86_64-darwin"
          "x86_64-linux"
        ]
          (system: let pkgs = import nixpkgs { inherit system; }; in f { inherit system pkgs; });
      mkModule = import ./nix/mkModule.nix;
    in
    {
      packages = eachSystem ({ pkgs, ... }: { default = pkgs.callPackage ./nix/package.nix { }; });

      nixosModules.default = mkModule { nixos = true; };
      nixosModules.gulp = self.nixosModules.default;

      homeManagerModules.default = mkModule { nixos = false; };
      homeManagerModules.gulp = self.homeManagerModules.default;

      formatter = eachSystem (
        { pkgs, ... }:
        treefmt-nix.lib.mkWrapper pkgs {
          projectRootFile = "flake.lock";
          programs.nixpkgs-fmt.enable = true;
          programs.clang-format.enable = true;
        }
      );

      devShells = eachSystem (
        { pkgs, ... }: {
          default = with pkgs;
            mkShell {
              buildInputs = [
                pandoc
                clang-tools

                # build deps
                xorg.libX11
                xorg.libXres
                procps
                pkg-config
                tomlc99
              ];
            };
        }
      );
    };
}
