# nix-shell --run ./build.sh   (MinGW-w64 cross toolchain with Windows headers)
with import <nixpkgs> {}; pkgsCross.mingwW64.mkShell { }
