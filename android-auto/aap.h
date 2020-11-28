// copied from https://github.com/viktorgino/libheadunit

#include <openssl/bio.h>
#include <openssl/ssl.h>

enum HU_FRAME_FLAGS
{
  HU_FRAME_FIRST_FRAME = 1 << 0,
  HU_FRAME_LAST_FRAME = 1 << 1,
  HU_FRAME_CONTROL_MESSAGE = 1 << 2,
  HU_FRAME_ENCRYPTED = 1 << 3,
};

enum class HU_INIT_MESSAGE : uint16_t
{
  VersionRequest = 0x0001,
  VersionResponse = 0x0002,
  SSLHandshake = 0x0003,
  AuthComplete = 0x0004,
};

enum State {
  handshake,
  connected
};

// Channels ( or Service IDs)
#define AA_CH_CTR 0                                                                                  // Sync with hu_tra.java, hu_aap.h and hu_aap.c:aa_type_array[]
#define AA_CH_TOU 1
#define AA_CH_SEN 2
#define AA_CH_VID 3
#define AA_CH_AUD 4
#define AA_CH_AU1 5
#define AA_CH_AU2 6
#define AA_CH_MIC 7
#define AA_CH_BT  8
#define AA_CH_PSTAT  9
#define AA_CH_NOT 10
#define AA_CH_NAVI 11
#define AA_CH_MAX 256

#define MAX_FRAME_PAYLOAD_SIZE 0x4000

typedef struct {
  uint16_t messageCode;
  uint8_t payload[6];
} versionReply;

class AAConnection;

class SslConnection {
public:
  SslConnection(AAConnection *parent);
  BIO *deviceToHost;
  BIO *hostToDevice;
  void incoming_ciphertext(const uint8_t *buffer, int size);
  void maybeSendOutgoingCiphertext(int fd_in);
private:
  const SSL_METHOD *methods;
  SSL_CTX *context;
  SSL *conn;
  AAConnection *parent;
};

class AAConnection {
public:
  AAConnection() : ssl(this) {};
  void sendUnencBlob(int fd_in, int channel, HU_INIT_MESSAGE messageCode, uint8_t *buffer, int size);
  void handle_incoming_packet(int fd_in, const uint8_t* buffer, int size);
  void handle_incoming_frame(int fd_in, const uint8_t* buffer, int size);
  void handleAuthComplete(const void *payload, int size);

  SslConnection ssl;
  State state;
};

extern AAConnection aa_conn; // TODO, make it more OO
