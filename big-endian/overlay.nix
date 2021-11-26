self: super: {
  systemd = super.systemd.override { withEfi = false; };
  makeModulesClosure = args: super.makeModulesClosure (args // {
    allowMissing = true;
  });
}
