{ nixos ? false }: { lib
                   , pkgs
                   , config
                   , ...
                   }:
with lib; let
  pkg = pkgs.callPackage ./package.nix { };
in
{
  options.services.gulp = {
    enable = mkEnableOption (lib.mdDoc "gulp");
    package = mkOption {
      type = types.package;
      default = pkg;
      description = "Underlying package to use";
    };
  };
  config =
    let
      homeConfiguration = {
        home.packages = [ pkg ];
        xdg.configFile."systemd/user/gulp.service".source = "${pkg}/etc/systemd/user/gulp.service";
      };
      nixosConfiguration = {
        environment.systemPackages = [ pkg ];
        systemd.packages = [ pkg ];
      };
    in
    mkIf config.services.gulp.enable
      (
        if nixos
        then nixosConfiguration
        else homeConfiguration
      );
}
