// usage: node sign input.bin output.bin
var fs = require("fs");
var keys = require("./keys");
var crypto = require("crypto");

// OTP values 19,20,21,22, in native byte order
// this converts them to a 16 byte blob, as it would have been in ram
var otp = Buffer.alloc(20,0);
for (var i=0; i<keys.otp.length; i++) {
  otp.writeUInt32LE(keys.otp[i], i*4);
}

// found at addresss 0x600035a4 on my maskrom
// `dw -b 0x600100f0 20` also reveals it in sram
var salt = Buffer.from(keys.salt);

var key = Buffer.alloc(20, 0);
for (var i=0; i<key.length; i++) {
  key[i] = salt[i] ^ otp[i];
}
console.log(key.toString("hex"));

console.log(process.argv);

var input = process.argv[2];
var output = process.argv[3];

var blob = fs.readFileSync(input);

var hmac = crypto.createHmac("sha1", key);
hmac.update(blob);
var signature = hmac.digest();

var signed = Buffer.concat([blob, signature]);
console.log("writing to",output);
fs.writeFileSync(output, signed);
