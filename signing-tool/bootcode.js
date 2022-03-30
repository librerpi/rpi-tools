const fs = require("fs");
var crypto = require("crypto");

const full_bootcode = fs.readFileSync(process.argv[2]);

const footer = full_bootcode.slice(full_bootcode.length - 284);

const payload_size = footer.readUint32LE(0);
console.log("payload size:", payload_size);
if ((payload_size + 284) == full_bootcode.length) {
  console.log("payload size is valid");
}

const key_index = footer.readUint32LE(4);
console.log("key index:", key_index);

const rsa_signature = footer.slice(8, 264);
console.log("rsa sig:", rsa_signature.toString("hex"));
// The RSA signature is interpreted as a 2048-bit big-endian integer, which is decrypted using the exponent value of 65537 and one of the 4 public key modulos and exporting the result again as a 2048-bit big-endian integer.

const hmac_sig = footer.slice(264, 284);
console.log("hmac sig:", hmac_sig.toString("hex"));

var keys = require("./keys");
const hmac_payload = full_bootcode.slice(0, full_bootcode.length - 20);


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
var key = Buffer.alloc(20, 0);
for (var i=0; i<key.length; i++) {
  key[i] = salt[i] ^ otp[i];
}

var hmac = crypto.createHmac("sha1", key);
hmac.update(hmac_payload);
const actual_hmac_sig = hmac.digest();
console.log("expected hmac sig:", actual_hmac_sig.toString("hex"));
