{ runCommand, eeprom-extractor, rpi-eeprom, time }:

runCommand "extracted" { buildInputs = [ eeprom-extractor time ]; } ''
  for channel in beta critical stable; do
    for file in ${rpi-eeprom}/firmware/$channel/pieeprom*.bin; do
      cp -v $file $channel-$(basename $file)
    done
  done
  mkdir $out
  cd $out
  for file in $NIX_BUILD_TOP/*.bin; do
    name=$(basename $file)
    name=''${name%.*}
    echo $file X ''$name
    mkdir $name
    pushd $name
    command time -v extractor $file
    popd
  done
''
