const fs = require("fs");
const bootcode = require("../signing-tool/bootcode.js");

function getStr(buf) {
  var len = buf.indexOf(0);
  return buf.toString("ascii", 0, len);
}

function process_eeprom(filename) {
  var res = filename.match(/beta|critical|stable/);
  var output = './';
  if (res) {
    output += res[0] + "/";
  }

  var res = filename.match(/(pieeprom-.*)\.bin/);
  if (res) {
    output += res[1] + "/";
  }

  console.log(output);
  fs.mkdirSync(output, {recursive: true});

  const full_eeprom = fs.readFileSync(filename);
  var pos = 0;
  while (pos < full_eeprom.length) {
    var magic = full_eeprom.readUint32BE(pos+0);
    var len = full_eeprom.readUint32BE(pos+4);
    var body = full_eeprom.slice(pos+8, pos+8+len);
    switch (magic) {
    case 0x55aaf00f:
      //console.log("bootcode.bin:", body);
      fs.writeFileSync(output+"bootcode.bin", body);
      var res = bootcode.parse_bootcode(body);
      fs.writeFileSync(output+"bootcode.info", JSON.stringify(res));
      break;
    case 0x55aaf11f:
      var name = getStr(body.slice(0,16));
      var real_body = body.slice(16);
      switch (name) {
      case "bootconf.txt":
        //console.log(name, real_body.toString("ascii"));
        break;
      default:
        //console.log('plain file', name, real_body);
      }
      fs.writeFileSync(output+name, real_body);
      break;
    case 0x55aaf33f:
      var name = getStr(body.slice(0,16));
      var real_body = body.slice(16, body.length - 32);
      var hash = body.slice(body.length - 32);
      //console.log("compressed", name, real_body, hash.toString("hex"));
      fs.writeFileSync(output+name+".compressed", real_body);
      fs.writeFileSync(output+name+".hash", hash);
      break;
    case 0x55aafeef:
      console.log(`padding, ${len} bytes`);
      break;
    case 0xffffffff:
      console.log(`EOF, ${full_eeprom.length - pos} bytes left`);
      return;
      break;
    default:
      console.log(magic.toString(16), len, body);
    }
    pos += ((8+len-1) & 0xffff8) + 8;
  }
}

process_eeprom(process.argv[2]);
