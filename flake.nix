{
  inputs = {
    nixpkgs.url = "github:NixOS/nixpkgs/nixos-unstable";
  };

  outputs = { self, nixpkgs, ... }:
  let
    allSystems = [ "x86_64-linux" "aarch64-linux" ];
    forAllSystems = f: nixpkgs.lib.genAttrs allSystems (system: f { pkgs = import nixpkgs { inherit system; }; });
  in
  {
    devShells = forAllSystems (
      { pkgs }: {
        default = pkgs.mkShell {
          buildInputs = [
            pkgs.cmake
            pkgs.gtest
            pkgs.gtest.dev
            pkgs.clang
            pkgs.ninja
          ];
        };
      }
    );
    packages = forAllSystems (
      { pkgs }: {
        default = pkgs.stdenv.mkDerivation {
          version = "0.1.0";
          src = ./.;
          nativeBuildInputs = [
            pkgs.cmake
            pkgs.ninja
          ];
          buildInputs = [
            pkgs.gtest
          ];
          installPhase = "cmake --install build";
        };
      }
    );
  };
}
