pself: psuper: {
  android-auto = pself.extend (self: super: {
    android-auto = self.callPackage ../android-auto {};
    initrd_script = ''
      modprobe -v dwc2
      modprobe -v gadgetfs
      mkdir /dev/gadget
      mount -t gadgetfs gadgetfs /dev/gadget

      openssl req -out test.x509 -newkey 2048 -keyout test.key -nodes -x509 -days 365 -subj '/commonName=testname/'
      export AAKEY=$(realpath test.key)
      export AACERT=$(realpath test.x509)
      android-auto
    '';
    scripts = self.runCommand "scripts" {} ''
      mkdir -pv $out/bin
      cp ${../android-auto/custom-gadget} $out/bin/custom-gadget
    '';
    extra_utils = [ self.android-auto self.openssl self.scripts ];
  });
}
