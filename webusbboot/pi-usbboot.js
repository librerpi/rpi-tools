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
  b.value = "push lk.bin";
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
  var cb = document.getElementById("autopush");
  if (cb.checked) connectToPi(dev);
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

fetchFirmware("lk.bin");
