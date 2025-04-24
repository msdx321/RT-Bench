{
  shared ? import ./common.nix,
}:
{
  default = shared.pkgs.mkShell {
    name = "rt-bench";
    nativeBuildInputs =
      with shared.pkgs;
      [
        binutils
        stdenv
        gcc
        glibc
        glibc.static
        json_c
      ]
      ++ shared.common;
    shellHook = ''
      export STATICX_LDD="${shared.pkgs.glibc.bin}/bin/ldd";
      echo "Development shell for rt-bench"
    '';
  };
}
