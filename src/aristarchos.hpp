#include "fits_header.hpp"
#include <vector>

/// @brief Size of command buffer to be sent to Aristarchos
constexpr int ARISTARCHOS_COMMAND_MAX_CHARS = 64;

/// @brief Ip for Aristarchos
constexpr char ARISTARCHOS_IP[] = "195.251.202.253";

/// @brief Port nr for connecting to Aristarchos
constexpr int ARISTARCHOS_PORT = 50001;

/// @brief Buffer size for communication with Aristarchos
// constexpr int ARISTARCHOS_MAX_SOCKET_BUFFER_SIZE = 4096;

/// @brief Max size of the FCC response (for the encrypted/compressed FITS
///        header buffer)
constexpr int ARISTARCHOS_MAX_HEADER_SIZE = 4096;

/// @brief Buffer size used for decoding (bzip2 && base64) of 1Mb
/// @todo is this too large?
// constexpr unsigned int ARISTARCHOS_DECODE_BUFFER_SIZE = 1024 * 1024;

int decoded_str_to_header(const char *decoded_msg, unsigned msg_len,
                          std::vector<FitsHeader> &header_vec) noexcept;

char *decode_message(char *raw_message, char *ascii_str, int buff_len,
                     unsigned &ascii_len) noexcept;

int get_aristarchos_headers(int num_tries,
                            std::vector<FitsHeader> &headers) noexcept;
