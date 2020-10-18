console.log("hello world");

var devices = {};
var payload;
var header;

navigator.usb.getDevices()
  .then(devs => {
    console.log("pre-authorized devices");
    console.log(devs);
    for (var dev of devs) {
      devices[dev.serialNumber] = { dev: dev, dom: createDevice(dev) };
    }
  })
  .catch(error => { console.log(error); });

function createDevice(dev) {
  var root = document.createElement("div");
  var t = document.createTextNode("pi: "+dev.serialNumber);
  root.appendChild(t);
  root.style.display = "block";
  root.style.border = "1px solid red";

  var b = document.createElement("input");
  b.type = "button";
  b.value = "push bootcode";
  b.onclick = function () {
    connectToPi(dev);
  };

  root.appendChild(b);

  document.getElementById("devices").appendChild(root);
  return root;
}
function flagDomFinished(dev) {
  var r = devices[dev.serialNumber].dom;
  var b = r.getElementsByTagName("input")[0];
  r.removeChild(b);
  r.appendChild(document.createElement("br"));
  r.appendChild(document.createTextNode("booting..."));
}

function doit() {
  var filters = [
    { vendorId: 0x0a5c, productId: 0x2711 } // pi4
  ];
  navigator.usb.requestDevice({ filters: filters })
    .then(dev => {
      console.log(dev);
      device = dev;
    })
    .catch(error => { console.log(error); });
}

navigator.usb.onconnect = function (ev) {
  console.log(ev);
  var dev = ev.device;
  devices[dev.serialNumber] = { dev: dev, dom: createDevice(dev) };
  if (dev.serialNumber == "Broadcom") {
    startFileserver(dev);
  } else {
    var cb = document.getElementById("autopush");
    if (cb.checked) connectToPi(dev);
  }
}

function ep_read(dev, size) {
  return dev.controlTransferIn({
    requestType: 'vendor',
    recipient: 'device',
    request: 0x0,
    value: size & 0xffff,
    index: (size >> 16) & 0xffff
  }, size)
    .then(result => {
      console.log(result);
      return result;
    })
}

function ep_write(dev, buf, size) {
  return control_out(dev, size)
    .then(() => {
      if (size > 0) {
        return dev.transferOut(1, buf);
      }
    })
}

const config_txt = "uart_2ndstage=1\n";

function checkFileserverRequest(dev, result) {
  var bit32view = new Uint32Array(result.data.buffer);
  var control = bit32view[0];
  console.log("control code", control);
  var namebuf = result.data.buffer.slice(4);
  var namelen = (new Uint8Array(namebuf)).indexOf(0);
  var filename = String.fromCharCode.apply(null, new Uint8Array(namebuf.slice(0,namelen)));
  console.log(filename);
  switch (control) {
  case 0: // get file size
    switch (filename) {
      case "config.txt":
        return control_out(dev, config_txt.length)
          .then(() => { return fileserverMain(dev); });
        break;
      default:
        return asyncFetch("usbboot/recovery/"+filename)
          .then(reply_body => {
            if (reply_body !== null) {
              console.log("reporting size:", filename, reply_body.byteLength);
              return control_out(dev, reply_body.byteLength)
                .then(() => { return fileserverMain(dev); });
            } else {
              console.log("file not found:", filename);
              return ep_write(dev, null, 0)
                .then(() => { return fileserverMain(dev); });
            }
          });
        break;
    }
    break;
  case 1: // read file
    console.log("file read:", filename);
    switch (filename) {
    case "config.txt":
      var rawbuf = str2ab(config_txt);
      return ep_write(dev, rawbuf, rawbuf.byteLength)
          .then(() => { return fileserverMain(dev); });
    default:
      return asyncFetch("usbboot/recovery/"+filename)
        .then(reply_body => {
          return ep_write(dev, reply_body, reply_body.byteLength)
            .then(() => { return fileserverMain(dev); });
        });
    }
    break;
  case 2: // done
  }
}

function str2ab(str) {
  var buf = new ArrayBuffer(str.length); // 2 bytes for each char
  var bufView = new Uint8Array(buf);
  for (var i=0, strLen=str.length; i < strLen; i++) {
    bufView[i] = str.charCodeAt(i);
  }
  return buf;
}


function control_out(dev, size) {
  return dev.controlTransferOut({
    requestType: 'vendor',
    recipient: 'device',
    request: 0x0,
    value: size & 0xffff,
    index: (size >> 16) & 0xffff
  })
  .then(result => {
    console.log("control out:", result);
  });
}

function startFileserver(dev) {
  dev
    .open()
    .then(() => dev.selectConfiguration(1))
    .then(() => {
      console.log("claiming for fileserver");
      return dev.claimInterface(0)
    })
    .then(() => { return fileserverMain(dev); })
    .catch(error => { console.log(error); });
}
function fileserverMain(dev) {
  return ep_read(dev, 260)
    .then(result => { return checkFileserverRequest(dev, result); })
}

navigator.usb.ondisconnect = function (ev) {
  console.log(ev);
  var dev = ev.device;
  if (devices[dev.serialNumber]) {
    devices[dev.serialNumber].dom.parentNode.removeChild(devices[dev.serialNumber].dom);
  }
}

function connectToPi(dev) {
  var header_length = header.byteLength;
  var length = payload.byteLength;
  var position = 0;
  function checkChunk(result) {
    console.log("range "+position+"-"+(position+result.bytesWritten)+" successfully sent");
    position += result.bytesWritten;
    if (position == payload.byteLength) {
      console.log("done");
      dev.controlTransferIn({
        requestType: 'vendor',
        recipient: 'device',
        request: 0x0,
        value: 4,
        index: 0,
      }, 4)
        .then(result => {
          console.log(result);
          var res2 = new Uint32Array(result.data);
          console.log(res2)
          dev.close();
          flagDomFinished(dev);
        })
      return;
    }
    return dev.transferOut(1, payload.slice(position, position+16384))
      .then(result => {
        return checkChunk(result);
      })
      .catch(error => { console.log(error); });
  }
  dev.open()
    .then(() => dev.selectConfiguration(1))
    .then(() => {
      console.log("claiming");
      return dev.claimInterface(0)
    })
    .then(() => {
      console.log("control 1");
      return dev.controlTransferOut({
        requestType: 'vendor',
        recipient: 'device',
        request: 0x0,
        value: header_length & 0xffff,
        index: (header_length >> 16) & 0xffff
      })
    }) // Ready to receive data
    .then(() => {
      console.log("header transfer");
      return dev.transferOut(1, header)
    })
    .then(headerResult => {
      console.log(headerResult);
      console.log("control 2");
      var obj = {
        requestType: 'vendor',
        recipient: 'device',
        request: 0x0,
        value: length & 0xffff,
        index: (length >> 16) & 0xffff
      };
      console.log(obj);
      return dev.controlTransferOut(obj);
    }) // Ready to receive data
    .then(control2Result => {
      console.log(control2Result);
      console.log("payload transfering...")
      return dev.transferOut(1, payload.slice(0, 16384))
    })
    .then(result => {
      return checkChunk(result);
    })
    .catch(error => { console.log(error); });
}

function fetchFirmware(relativepath) {
  var oReq = new XMLHttpRequest();
  oReq.open("GET", relativepath, true);
  oReq.responseType = "arraybuffer";
  oReq.onload = function (oEvent) {
    var arrayBuffer = oReq.response; // Note: not oReq.responseText
    if (arrayBuffer) {
      console.log(arrayBuffer);
      console.log("fetched "+arrayBuffer.byteLength+" bytes of "+relativepath);
      payload = arrayBuffer;
      header = new ArrayBuffer(24);
      new DataView(header).setInt32(0, arrayBuffer.byteLength, true);
    }
  };
  oReq.send(null);
}

function asyncFetch(path) {
  return new Promise(function (resolve, reject) {
    var oReq = new XMLHttpRequest();
    oReq.open("GET", path, true);
    oReq.responseType = "arraybuffer";
    oReq.onload = function (oEvent) {
      if (oReq.status == 200) {
        var arrayBuffer = oReq.response; // Note: not oReq.responseText
        if (arrayBuffer) {
          console.log(arrayBuffer);
          console.log("fetched "+arrayBuffer.byteLength+" bytes of "+path);
          resolve(arrayBuffer);
        }
      } else {
        resolve(null);
      }
    };
    oReq.send(null);
  });
}

fetchFirmware("lk.bin");
