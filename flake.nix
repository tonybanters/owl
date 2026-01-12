{
  description = "owl - Open Wayland Library";

  inputs = {
    nixpkgs.url = "github:NixOS/nixpkgs/nixos-unstable";
  };

  outputs = {
    self,
    nixpkgs,
  }: let
    systems = ["x86_64-linux" "aarch64-linux"];
    forAllSystems = fn: nixpkgs.lib.genAttrs systems (system: fn nixpkgs.legacyPackages.${system});
  in {
    devShells = forAllSystems (pkgs: {
      default = pkgs.mkShell {
        buildInputs = with pkgs; [
          gcc
          gnumake
          pkg-config
          bear

          wayland
          wayland-protocols
          wayland-scanner

          libdrm
          libinput
          libxkbcommon
          systemdLibs

          libGL
          mesa
          libgbm
          pixman
          linuxHeaders
        ];

        nativeBuildInputs = with pkgs; [
          pkg-config
        ];

        shellHook = ''
          echo "owl development environment loaded"
          echo ""
          echo "  make           - build libowl.a"
          echo "  make examples  - build simple_wm example"
          echo "  make clean     - clean build artifacts"
        '';
      };
    });

    formatter = forAllSystems (pkgs: pkgs.alejandra);
  };
}
