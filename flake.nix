# SPDX-License-Identifier: MIT
{
  description = "Grand Time Auto - Sonata v1.3 slot-2 wallpaper and driving game";

  inputs = {
    sonata-software.url = "git+https://github.com/lowRISC/sonata-software.git?ref=v1.3&rev=722cdea0207920997b0895ea1f430619b343e10c&submodules=1";
    nixpkgs.follows = "sonata-software/nixpkgs";
  };

  nixConfig = {
    extra-substituters = ["https://nix-cache.lowrisc.org/public/"];
    extra-trusted-public-keys = [
      "nix-cache.lowrisc.org-public-1:O6JLD0yXzaJDPiQW1meVu32JIDViuaPtGDfjlOopU7o="
    ];
  };

  outputs = {
    sonata-software,
    nixpkgs,
    ...
  }: let
    system = "x86_64-linux";
    pkgs = import nixpkgs {inherit system;};
    imagePython = pkgs.python3.withPackages (ps: [ps.pillow]);
    withWallpaperEnv = shell:
      shell.overrideAttrs (old: {
        nativeBuildInputs = [imagePython] ++ (old.nativeBuildInputs or []);
        SONATA_SOFTWARE = "${sonata-software}";
        GTA6_WALLPAPER_SHELL = "1";
      });
  in {
    devShells.${system} = {
      default = withWallpaperEnv sonata-software.devShells.${system}.default;
      sim = withWallpaperEnv sonata-software.devShells.${system}.env-with-sim;
      preview = pkgs.mkShell {
        packages = [
          pkgs.emscripten
          pkgs.stdenv.cc
          (pkgs.python3.withPackages (ps: [ps.playwright ps.pillow ps.qrcode]))
          pkgs.chromium
          pkgs.zbar
          pkgs.iproute2
        ];
        GTA6_PREVIEW_SHELL = "1";
        CHROMIUM = "${pkgs.chromium}/bin/chromium";
      };
    };
    formatter.${system} = pkgs.alejandra;
  };
}
