
/// @brief Size of command buffer to be sent to Aristarchos
constexpr int ARISTARCHOS_COMMAND_MAX_CHARS = 64;

/// @brief Ip for Aristarchos
constexpr char ARISTARCHOS_IP[] = "195.251.202.253";

/// @brief Port nr for connecting to Aristarchos
constexpr int ARISTARCHOS_PORT = 50001;

/// @brief Buffer size for communication with Aristarchos
constexpr int ARISTARCHOS_DECODE_BUFFER_SIZE = 1024;

/// @brief Buffer size used for decoding (bzip2 && base64)
constexpr unsigned int ARISTARCHOS_

char *generate_request_string(const char* request, char *command) noexcept;

int send_aristarchos_request(int delay_sec, const char* request, int need_reply, char *reply) noexcept;