rec {
  # pinned version of nixpkgs for development dependencies
  pkgs = import (fetchTarball {
    url = "https://github.com/NixOS/nixpkgs/archive/888e0ce8350032a83abd621c6d3d341c5c954887.tar.gz";
    sha256 = "sha256:1jdrpp4ac5m3vx9yfadvsr7kfrc80q9jp1lyj66jspkqb47fbazb";
  }) { };
  # aarch64 cross compile packages
  pkgs-aarch64 = import (pkgs.fetchFromGitHub {
    owner = "NixOS";
    repo = "nixpkgs";
    rev = "888e0ce8350032a83abd621c6d3d341c5c954887";
    hash = "sha256:1jdrpp4ac5m3vx9yfadvsr7kfrc80q9jp1lyj66jspkqb47fbazb";
  }) { crossSystem.config = "aarch64-unknown-linux-gnu"; };
  #the docker ubuntu images used for CI/CD doxygen 1.9.8, so we use the same
  pkgs-doxygen = import (pkgs.fetchFromGitHub {
    owner = "NixOS";
    repo = "nixpkgs";
    rev = "dd5621df6dcb90122b50da5ec31c411a0de3e538";
    hash = "sha256-WVz9WdaFBhAwO/7A+HlW8HPJ4VQ8QnpCD1WZAcAPneo=";
  }) { };
  common = with pkgs; [
      treefmt
      pre-commit
      util-linux
      gnugrep
      coreutils
      ps
      cloc
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
      pkgs-doxygen.doxygen
  ];
}
