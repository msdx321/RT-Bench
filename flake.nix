{
  description = "A reproducible environment for rt-bench";
  inputs = {
    # pinnd version of nixpkgs for development dependencies
    nixpkgs.url =
      "github:nixos/nixpkgs/888e0ce8350032a83abd621c6d3d341c5c954887";
    flake-parts.url = "github:hercules-ci/flake-parts";
    treefmt-nix = {
      url = "github:numtide/treefmt-nix";
      inputs.nixpkgs.follows = "nixpkgs";
    };
    pre-commit-hooks = {
      url = "github:cachix/pre-commit-hooks.nix";
      inputs = { nixpkgs.follows = "nixpkgs"; };
    };
  };
  outputs = inputs@{ self, nixpkgs, flake-parts, treefmt-nix, systems, ... }:
    flake-parts.lib.mkFlake { inherit inputs; } {
      imports =
        [ inputs.treefmt-nix.flakeModule inputs.pre-commit-hooks.flakeModule ];
      systems = [ "x86_64-linux" "aarch64-linux" ];
      perSystem = attrs@{ config, pkgs, system, ... }:
        with attrs; {

          # Eval the treefmt modules
          treefmt = {
            # Used to find the project root
            projectRootFile = "flake.nix";
            programs = {
              black.enable = true;
              isort.enable = true;
              nixfmt.enable = true;
              prettier.enable = true;
              shfmt.enable = true;
              clang-format.enable = true;
            };
          };
          pre-commit = {
            check.enable = true;
            settings = {
              hooks = {
                treefmt.enable = true;
                convco.enable = true;
              };
              settings.treefmt = { package = config.treefmt.build.wrapper; };
            };
          };
          devShells = let
            # we don't want to use the default nixpkgs instance, but the pinned one for the development environment
            pkgs-aarch64 = import nixpkgs {
              inherit system;
              crossSystem.config = "aarch64-unknown-linux-gnu";
            };
            native_packages = with pkgs; [
              config.treefmt.build.wrapper
              util-linux
              gnugrep
              coreutils
              ps
              cloc
              doxygen
              graphviz
              gnumake
              git
              gnused
              imagemagick
              bash
              perl
              bashInteractive
              patchelf
              python3
              python3Packages.pip
              python3Packages.matplotlib
              python3Packages.pandas
              python3Packages.numpy
            ];
          in {
            default = pkgs.mkShell {
              name = "rt-bench";
              nativeBuildInputs = native_packages
                ++ [ pkgs.gcc pkgs.glibc pkgs.glibc.static pkgs.json_c ];
              STATICX_LDD = "${pkgs.glibc.bin}/bin/ldd";
            };
            aarch64 = pkgs.mkShell {
              name = "rt-bench-cross-aarch64";
              nativeBuildInputs = [
                pkgs-aarch64.buildPackages.gcc
                pkgs-aarch64.glibc
                pkgs-aarch64.glibc.static
                pkgs-aarch64.json_c
              ] ++ native_packages;
              STATICX_LDD = "${pkgs-aarch64.glibc.bin}/bin/ldd";
              shellHook = ''
                echo "Cross-compilation environment for aarch64"
              '';
            };
          };
        };
    };
}
