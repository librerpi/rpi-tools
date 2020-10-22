var devices = {};
var payload;
var header;
const config_txt = "uart_2ndstage=1\nenable_uart=1\n";
var fileserv_root = "usbboot/recovery/";

function updateRoot() {
  switch (document.getElementById("rootdir").value) {
  case "recovery":
    fileserv_root = "usbboot/recovery/";
    break;
  case "msd":
    fileserv_root = "usbboot/msd/";
    break;
  case "linux1":
    fileserv_root = "linux1/";
    break;
  }
}

function updateBootcode() {
  switch (document.getElementById("bootcode").value) {
  case "lk":
    fetchFirmware('lk.bin');
    break;
  case "recovery":
    fetchFirmware('usbboot/recovery/bootcode4.bin');
    break;
  }
}

navigator.usb.getDevices()
  .then(devs => {
    console.log("pre-authorized devices", devs);
    for (var dev of devs) {
      showLog(dev.serialNumber + ": device found on startup");
      handleNewDevice(dev);
    }
  })
  .catch(error => { console.log(error); });

navigator.usb.onconnect = function (ev) {
  var dev = ev.device;
  showLog(dev.serialNumber + ": hotplug detected");
  handleNewDevice(dev);
}

navigator.usb.ondisconnect = function (ev) {
  var dev = ev.device;
  if (devices[dev.serialNumber]) {
    devices[dev.serialNumber].dom.parentNode.removeChild(devices[dev.serialNumber].dom);
  }
}

function handleNewDevice(dev) {
  devices[dev.serialNumber] = { dev: dev, dom: createDevice(dev) };
  if (dev.serialNumber == "Broadcom") {
    var cb = document.getElementById("autopush");
    if (cb.checked) startFileserver(dev);
  } else {
    var cb = document.getElementById("autopush");
    if (cb.checked) connectToPi(dev);
  }
}

function createDevice(dev) {
  var root = document.createElement("div");
  var t = document.createTextNode("pi: "+dev.serialNumber);
  root.appendChild(t);
  root.className = "device";

  var b = document.createElement("input");
  b.type = "button";
  if (dev.serialNumber == "Broadcom") {
    b.value = "start fileserver";
    b.onclick = function () {
      startFileserver(dev);
    };
  } else {
    b.value = "push bootcode";
    b.onclick = function () {
      connectToPi(dev);
    };
  }

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

function showLog(msg) {
  var node = document.createElement("div");
  node.innerText = msg;
  document.getElementById("console").appendChild(node);
  try {
    node.scrollIntoView();
  } catch (e) {
  }
}

function requestAccess() {
  var filters = [
    { vendorId: 0x0a5c, productId: 0x2711 }, // pi4 ROM, bootcode.bin, and recovery.bin
    { vendorId: 0x0a5c, productId: 0x2764 } // pi4 start4.elf
  ];
  navigator.usb.requestDevice({ filters: filters })
    .then(dev => {
      showLog(dev.serialNumber + ": access granted");
      handleNewDevice(dev);
    })
    .catch(error => { console.log(error); });
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
      if (result.status != "ok") throw new Error("ep_read failed");
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

function checkFileserverRequest(dev, result) {
  var bit32view = new Uint32Array(result.data.buffer);
  var control = bit32view[0];
  var namebuf = result.data.buffer.slice(4);
  var namelen = (new Uint8Array(namebuf)).indexOf(0);
  var filename = String.fromCharCode.apply(null, new Uint8Array(namebuf.slice(0,namelen)));
  switch (control) {
  case 0: // get file size
    switch (filename) {
      case "config.txt":
        return control_out(dev, config_txt.length)
          .then(() => { return fileserverMain(dev); });
        break;
      default:
        return asyncFetch(fileserv_root + filename)
          .then(reply_body => {
            if (reply_body !== null) {
              showLog(dev.serialNumber+": sending size of "+filename+" as "+reply_body.byteLength);
              return control_out(dev, reply_body.byteLength);
            } else {
              showLog(dev.serialNumber+": file not found: "+filename);
              return ep_write(dev, null, 0);
            }
          });
        break;
    }
    break;
  case 1: // read file
    showLog(dev.serialNumber + ": file read " + filename);
    switch (filename) {
    case "config.txt":
      var rawbuf = str2ab(config_txt);
      return ep_write(dev, rawbuf, rawbuf.byteLength);
    default:
      return asyncFetch(fileserv_root + filename)
        .then(reply_body => {
          return ep_write(dev, reply_body, reply_body.byteLength);
        });
    }
    break;
  case 2: // done
    console.log("DONE!");
    dev.close();
    break;
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
    if (result.status != "ok") throw new Error("control out failed");
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
    .catch(error => {
      console.log(error);
    });
}
function fileserverMain(dev) {
  return ep_read(dev, 260)
    .then(result => { return checkFileserverRequest(dev, result); })
    .then(() => { return fileserverMain(dev); });
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
  asyncFetch(relativepath)
    .then(arrayBuffer => {
      console.log("fetched "+relativepath);
      payload = arrayBuffer;
      header = new ArrayBuffer(24);
      new DataView(header).setInt32(0, arrayBuffer.byteLength, true);
    });
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
          //console.log("fetched "+arrayBuffer.byteLength+" bytes of "+path);
          resolve(arrayBuffer);
        }
      } else {
        resolve(null);
      }
    };
    oReq.send(null);
  });
}

setTimeout(function () {
  updateBootcode();
  updateRoot();
}, 10);
