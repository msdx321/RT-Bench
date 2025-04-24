{
  shared ? import ./common.nix,
}:
{
  default = shared.pkgs.mkShell {
    name = "rt-bench";
    nativeBuildInputs =
      with shared;
      [
        pkgs-aarch64.buildPackages.binutils
        pkgs-aarch64.buildPackages.stdenv.cc
        pkgs-aarch64.buildPackages.gcc
        pkgs-aarch64.glibc
        pkgs-aarch64.glibc.static
        pkgs-aarch64.json_c
      ]
      ++ common;
    shellHook = ''
      echo "Aarch64 cross compiling development shell for rt-bench"
    '';
  };
}
