{
  inputs = {
    nixpkgs.url = "github:NixOS/nixpkgs/nixos-25.05";
  };

  outputs = { self, nixpkgs, ... }:
  let
    allSystems = [ "x86_64-linux" "aarch64-linux" ];
    forAllSystems = f: nixpkgs.lib.genAttrs allSystems (system: f
    {
      pkgs = import nixpkgs { inherit system; };
      system = system;
    });
  in
  {
    packages = forAllSystems (
      { pkgs, system }: {
        default = pkgs.llvmPackages_20.stdenv.mkDerivation {
          name = "schtest";
          version = "0.1.0";
          src = ./.;
          nativeBuildInputs = [
            pkgs.llvmPackages_20.clang-tools
            pkgs.cmake
            pkgs.ninja
            pkgs.pkg-config
          ];
          buildInputs = [
            pkgs.boost
            pkgs.folly
            pkgs.gflags
            pkgs.glog
            pkgs.gtest
            pkgs.jemalloc
          ];
          installPhase = "cmake --install build";
        };
      }
    );
    devShells = forAllSystems (
      { pkgs, system }: {
        default = pkgs.mkShell {
          buildInputs = self.packages.${system}.default.nativeBuildInputs ++ self.packages.${system}.default.buildInputs;
        };
      }
    );
  };
}
