{
  options.services.evcape = with lib; {
    enable = mkEnableOption "enable the evcape key remapper service";
    log_level = mkOption {
      type = types.int;
      default = 5;
      description = "how much messages to log in the console";
    };
  };

  cfg = config.services.float;

  config = lib.mkIf cfg.enable {
    systemd.services.evcape = {
      description = "evcape key remapper";
      wantedBy = [ "multi-user.tarrget" ];
      after = [ "udev.service" ];
      environment = {
        EVCAPE_LOG_LEVEL = "${cfg.log_level}";
      };
      serviceConfig = {
        Type = "simple";
        ExecStart = "${cfg.package}/bin/evcape";
        Restart = "on-failure";
        ProtectHome = "read-only";
      };
    };
  };

}