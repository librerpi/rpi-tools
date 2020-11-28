#include <arpa/inet.h>
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include "aap.h"
#include "pb/hu.pb.h"

static void handleVersionRequest(int fd_in, const uint8_t* buffer, int size);
static void handleSslHandshake(int fd_in, const uint8_t* buffer, int size);
static void sendPacket(int fd_in, uint8_t *buffer, int size, int channel);

SslConnection::SslConnection(AAConnection *parent) : parent(parent) {
  parent->state = handshake;
  int ret;

  ERR_load_BIO_strings();
  ERR_load_SSL_strings();
  SSL_library_init();
  OPENSSL_add_all_algorithms_noconf();

  deviceToHost = BIO_new(BIO_s_mem());
  if (deviceToHost == NULL) {
    puts("BIO_new() failed");
  }
  hostToDevice = BIO_new(BIO_s_mem());
  if (hostToDevice == NULL) {
    puts("BIO_new() failed");
  }
  methods = TLS_server_method();
  assert(methods != NULL);

  context = SSL_CTX_new(methods);
  assert(context != NULL);

  ret = SSL_CTX_use_certificate_chain_file(context, getenv("AACERT"));
  assert(ret == 1);
  ret = SSL_CTX_use_PrivateKey_file(context, getenv("AAKEY"), SSL_FILETYPE_PEM);
  assert(ret == 1);

  ret = SSL_CTX_check_private_key (context);
  assert(ret == 1);

  conn = SSL_new(context);
  assert(conn != NULL);

  //SSL_set_msg_callback(conn, SSL_trace);
  //SSL_set_msg_callback_arg(conn, BIO_new_fp(stdout,0));

  SSL_set_bio(conn, hostToDevice, deviceToHost);
  BIO_set_write_buf_size(deviceToHost, MAX_FRAME_PAYLOAD_SIZE);
  BIO_set_write_buf_size(hostToDevice, MAX_FRAME_PAYLOAD_SIZE);

  puts("accept begin");
  SSL_accept(conn);
  puts("acceept end");

  ret = SSL_do_handshake(conn);
  printf("handshake returned %d\n", ret);
}

void SslConnection::incoming_ciphertext(const uint8_t *buffer, int size) {
  int ret = BIO_write(hostToDevice, buffer, size);
  assert(ret == size);
  if (parent->state == handshake) {
    puts("doing handshake step");
    ret = SSL_do_handshake(conn);
    printf("done %d\n", ret);
  }
}

uint16_t be16toh16(uint16_t x) {
  return ((x & 0xff) << 8) | (x >> 8);
}

AAConnection aa_conn; // TODO, make it more OO

void AAConnection::handle_incoming_frame(int fd_in, const uint8_t* buffer, int size) {
  uint8_t channel = buffer[0];
  uint8_t flags = buffer[1];
  printf("    channel %d flags %d\n", channel, flags);
  if (flags == (HU_FRAME_FIRST_FRAME | HU_FRAME_LAST_FRAME)) {
    puts("    frame contains full packet");
    uint16_t frame_size = be16toh16(*((const uint16_t*) (buffer + 2)));
    handle_incoming_packet(fd_in, buffer + 4, frame_size);
  } else {
    puts("    warning, unable to support this flag combination");
  }
}

void AAConnection::handle_incoming_packet(int fd_in, const uint8_t* buffer, int size) {
  HU_INIT_MESSAGE messageCode = (HU_INIT_MESSAGE)be16toh16(*((const uint16_t*)buffer));
  const uint8_t *payload = buffer + 2;
  printf("messageCode: %d %d\n", (uint32_t)messageCode, size);
  switch (messageCode) {
  case HU_INIT_MESSAGE::VersionRequest:
    handleVersionRequest(fd_in, payload, size-2);
    break;
  case HU_INIT_MESSAGE::SSLHandshake:
    handleSslHandshake(fd_in, payload, size-2);
    break;
  case HU_INIT_MESSAGE::AuthComplete:
    // payload is a `message AuthCompleteResponse` from the protobuf
    // past this point, SSL is online, any frame with HU_FRAME_ENCRYPTED gets shoved into hostToDevice
    // SSL_read is then used to pull the plaintext out the other end, size is 1:1
    // frames are still re-assembled into packets, as before
    handleAuthComplete(payload, size-2);
    break;
  default:
    puts("unhandled message code");
  }
}

void AAConnection::handleAuthComplete(const void *payload, int size) {
  HU::AuthCompleteResponse res;
  if (res.ParseFromArray(payload, size)) {
    puts("valid protobuf");
    printf("status: %d\n", (uint32_t)res.status());
  }
  state = connected;
}

static void handleVersionRequest(int fd_in, const uint8_t* buffer, int size) {
  printf("version request: ");
  for (int i=0; i<size; i++) printf(" 0x%02x", buffer[i]);
  puts("");
  versionReply reply;
  reply.messageCode = htons((uint16_t)HU_INIT_MESSAGE::VersionResponse);
  reply.payload[0] = 0;
  reply.payload[1] = 1;
  reply.payload[2] = 0;
  reply.payload[3] = 6;
  reply.payload[4] = 0;
  reply.payload[5] = 0;
  sendPacket(fd_in, (uint8_t*)&reply, sizeof(reply), AA_CH_CTR);
}

static void sendPacket(int fd_in, uint8_t *buffer, int size, int channel) {
  uint8_t out[size+4];
  uint8_t flags = HU_FRAME_FIRST_FRAME | HU_FRAME_LAST_FRAME;
  out[0] = channel;
  out[1] = flags;
  *(uint16_t*)(&out[2]) = htons(size);
  for (int i=0; i<size; i++) {
    out[4+i] = buffer[i];
  }
  int ret = write(fd_in, out, size+4);
  assert(ret == size+4);
  printf("sent %d byte reply\n", ret);
}

static void handleSslHandshake(int fd_in, const uint8_t* buffer, int size) {
  aa_conn.ssl.incoming_ciphertext(buffer, size);
}

void SslConnection::maybeSendOutgoingCiphertext(int fd_in) {
  int pending = BIO_ctrl_pending(deviceToHost);
  printf("BIO_ctrl_pending %d\n", pending);
  printf("BIO_ctrl_wpending %d\n", BIO_ctrl_wpending(deviceToHost));
  if (pending > 0) {
    uint8_t buffer[pending];
    int ret = BIO_read(deviceToHost, buffer, pending);
    assert(ret == pending);
    parent->sendUnencBlob(fd_in, AA_CH_CTR, HU_INIT_MESSAGE::SSLHandshake, buffer, pending);
  }
}

void AAConnection::sendUnencBlob(int fd_in, int channel, HU_INIT_MESSAGE messageCode, uint8_t *buffer, int size) {
  uint8_t packet[size+2];
  *(uint16_t*)packet = htons((uint16_t)messageCode);
  memcpy(packet+2, buffer, size);
  sendPacket(fd_in, packet, size+2, channel);
}
