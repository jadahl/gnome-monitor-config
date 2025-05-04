{
  description = "Gnome monitor config CLI tool";

  inputs = {
    nixpkgs.url = "github:NixOS/nixpkgs";
  };

  outputs = { self, nixpkgs }:
    let
      pname = "gnome-monitor-config";
      supportedSystems = [ "x86_64-linux" "aarch64-linux" ];
      forAllSystems = function:
        nixpkgs.lib.genAttrs supportedSystems (system:
          function { 
            inherit system;
            pkgs = import nixpkgs { inherit system; };
          }
        );
    in {
      packages = forAllSystems ({ pkgs, ... }:
        {
          default = pkgs.clangStdenv.mkDerivation {
            inherit pname;
            version = "1.0.0";
            src = ./.;

            nativeBuildInputs = with pkgs; [
              cmake
              meson
              ninja
              pkg-config
            ];

            buildInputs = with pkgs; [
              cairo
            ];

            configurePhase = ''
              meson build
            '';

            buildPhase = ''
              (
                cd build
                meson compile
              )
            '';

            installPhase = ''
              mkdir -p $out/bin
              cp build/src/${pname} $out/bin
            '';
          };
        }
      );

      apps = forAllSystems ({ system, ... }: {
        default = {
          type = "app";
          program = "${self.packages.${system}.default}/bin/${pname}";
        };
      });

      devShells = forAllSystems ({ system, pkgs, ... }:
        let
          llvm = pkgs.llvmPackages_latest;
        in {
          default = pkgs.mkShell.override { stdenv = pkgs.clangStdenv; } {
            packages =
              self.packages.${system}.default.nativeBuildInputs ++
              self.packages.${system}.default.buildInputs ++
              (with pkgs; [
                # debugger
                llvm.lldb
                gdb

                # fix headers not found
                clang-tools

                # LSP and compiler
                # llvm.libstdcxxClang

                # other tools
                # cppcheck
                llvm.libllvm
                valgrind

                # stdlib for cpp
                # llvm.libcxx
              ]);
          };
        });
    };
}
