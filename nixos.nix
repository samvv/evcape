{ config, lib, pkgs, ... }: with lib;

  let cfg = config.services.evcape; in {

    options.services.evcape = {
      enable = mkEnableOption "enable the evcape key remapper service";
      log_level = mkOption {
        type = types.int;
        default = 5;
        description = "how much messages to log in the console";
      };
    };

    config = mkIf cfg.enable {
      systemd.services.evcape = {
        description = "evcape key remapper";
        wantedBy = [ "multi-user.tarrget" ];
        after = [ "udev.service" ];
        environment = {
          EVCAPE_LOG_LEVEL = "${cfg.log_level}";
        };
        serviceConfig = {
          Type = "simple";
          ExecStart = "${self.packages.${system}.evcape}/bin/evcape";
          Restart = "on-failure";
          ProtectHome = "read-only";
        };
      };
    };

  }
