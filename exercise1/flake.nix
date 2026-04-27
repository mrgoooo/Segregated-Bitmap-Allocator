{
  description = "Bitmap allocator exercise";

  inputs = {
    nixpkgs.url = "github:NixOS/nixpkgs/nixos-unstable";
    flake-utils.url = "github:numtide/flake-utils";
  };

  outputs = { self, nixpkgs, flake-utils }:
    flake-utils.lib.eachDefaultSystem (system:
      let
        pkgs = nixpkgs.legacyPackages.${system};
      in
      {
        packages = {
          default = self.packages.${system}.bitmap-alloc;
          
          bitmap-alloc = pkgs.stdenv.mkDerivation {
            name = "bitmap-alloc";
            src = ./.;
            
            nativeBuildInputs = with pkgs; [
              cmake
            ];
            
            buildInputs = with pkgs; [
              gtest
              gbenchmark
            ];
            
            configurePhase = ''
              cmake -B build 
            '';
            
            buildPhase = ''
              cmake --build build
            '';
            
            installPhase = ''
              mkdir -p $out/bin
              cmake --install build --prefix $out
            '';
          };
        };
        
        devShells.default = pkgs.mkShell {
          packages = with pkgs; [
            cmake
            gcc
            gnumake
            gdb
            gtest
            gbenchmark
            clang-tools
            samply
          ];
          NIX_ENFORCE_NO_NATIVE=false;
        };
      }
    );
}
