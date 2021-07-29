#include <vector>

/// @brief Size of command buffer to be sent to Aristarchos
constexpr int ARISTARCHOS_COMMAND_MAX_CHARS = 64;

/// @brief Ip for Aristarchos
constexpr char ARISTARCHOS_IP[] = "195.251.202.253";

/// @brief Port nr for connecting to Aristarchos
constexpr int ARISTARCHOS_PORT = 50001;

/// @brief Buffer size for communication with Aristarchos
constexpr int ARISTARCHOS_SOCKET_BUFFER_SIZE = 1024;

/// @brief Buffer size used for decoding (bzip2 && base64) of 1Mb
/// @todo is this too large?
constexpr unsigned int ARISTARCHOS_DECODE_BUFFER_SIZE = 1024 * 1024;

struct AristarchosHeader {
  char key[16];
  char val[32];
  char comment[64];
};

char *generate_request_string(const char *request, char *command) noexcept;

int send_aristarchos_request(int delay_sec, const char *request, int need_reply,
                             char *reply) noexcept;

char *uncompress_bz2_string(char *source, char *dest,
                            unsigned int &destLen) noexcept;

char *unbase64(const char *source, char *decoded) noexcept;

char *add_char_every(const char *source, char *dest, int every,
                     char delim) noexcept;

char *decode_message(const char *message) noexcept;