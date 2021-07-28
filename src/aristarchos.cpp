#include "aristarchos.hpp"
#include "andor2k.hpp"
#include "cpp_socket.hpp"
#include <bit>
#include <bzlib.h>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <openssl/bio.h>
#include <openssl/buffer.h>
#include <openssl/evp.h>
#include <openssl/hmac.h>
#include <openssl/sha.h>
#include <thread>

using andor2k::ClientSocket;
using andor2k::Socket;

/// @brief a pretty big char buffer for bzip2 decompressing
char str_buffer_long[ARISTARCHOS_DECODE_BUFFER_SIZE];

/// @brief for ceil_power2 we need the following to hold:
static_assert(sizeof(unsigned int) == 4);

/// @brief Compute the nearest power of two, not less than v
unsigned int ceil_power2(unsigned int v) noexcept {
  v--;
  v |= v >> 1;
  v |= v >> 2;
  v |= v >> 4;
  v |= v >> 8;
  v |= v >> 16;
  v++;
  return v;
}

/// @brief  base64 decode a given string
/// This function will use the SSL BIO lib to decode a string in base64
/// encryption format.
/// @param[in] source string to decode; must be null terminated
/// @param[out] decoded buffer to place the decoded message; must be at least
///             of the same size as the input string
/// @return a pointer to decoded (the null terminated decoded string)
char *unbase64(const char *source, char *decoded) noexcept {
  BIO *b64, *bmem;

  int length = std::strlen(source);
  std::memset(decoded, '\0', length + 1);

  // char* str = std::strdup(source);
  char *str = new char[length + 1];
  std::memset(str, '\0', length + 1);
  std::strcpy(str, source);

  b64 = BIO_new(BIO_f_base64());
  bmem = BIO_new_mem_buf(str, length);
  bmem = BIO_push(b64, bmem);

  BIO_read(bmem, decoded, length);
  BIO_free_all(bmem);

  delete[] str;

  return decoded;
}

/// @brief Add char after every n characters in string
/// This function will create a new copy of the string source, where after every
/// every character an extra delim character is added. A delim character is also
/// added at the end of the string. The new string is null terminated.
/// Example (every=6, delim='-'):
/// Original string [ab] of size 2, becomes: [ab+]
/// Original string [abcdef] of size 6, becomes: [abcdef+]
/// Original string [abcdefg] of size 7, becomes: [abcdef+g+]
/// Original string [abcdefghijklmnopqrstuvwxy] of size 25, becomes:
/// [abcdef+ghijkl+mnopqr+stuvwx+y+]
/// @param[in] source the original, null-terminated string
/// @param[out] dest the resulting string; the size of the array must be large
///             enough to hold the result string
/// @param[in] every Add the delim char after every this number of characters
/// @param[in] delim character to add
/// @return A pointer to the dest string
char *add_char_every(const char *source, char *dest, int every,
                     char delim) noexcept {
  int str_length = std::strlen(source);
  int add_nr = str_length / every + (str_length % every != 0);
  char *at = dest;
  int chars_added;
  for (int i = 0; i < add_nr; i++) {
    int from = every * i;
    std::strncpy(at, source + from, every);
    chars_added = (i + 1) * every < str_length ? every : str_length - i * every;
    at += chars_added;
    *at = delim;
    ++at;
  }
  *at = '\0';

  return dest;
}

char *decode_message(const char *message) noexcept {

  char buf[32];

  // Find the start of the block. This is usually BF=[B64....];
  const char *start = std::strstr(message, "B64");
  if (!start) {
    fprintf(stderr,
            "[ERROR][%s] Failed to decode message; could not find start of "
            "block \"B64\" (traceback: %s)\n",
            date_str(buf), __func__);
    return nullptr;
  }
  // skip "B64" part
  start += 3;

  // Find the end of the block, aka the ';' character
  const char *end = start;
  while (*end && *end != ';')
    ++end;
  if (!*end) {
    fprintf(stderr,
            "[ERROR][%s] Failed to decode message; could not find end of block "
            "\";\" (traceback: %s)\n",
            date_str(buf), __func__);
    return nullptr;
  }

  // length of block (without the semicolon)
  int block_sz = end - start;

  // check the block size
  if (block_sz < 100) {
    fprintf(stderr,
            "[ERROR][%s] Failed to decode message; block too small (traceback: "
            "%s)\n",
            date_str(buf), __func__);
    return nullptr;
  }

  // create a copy of the encoded string where a newline character is added
  // after every 64 chars
  /* allocate str_wnl */
  /* int new_str_sz = std::bit_ceil(block_sz + (block_sz / 64) + 2);
   * unfortunately, only available in C++20
   */
  int approx_new_sz = block_sz + block_sz/64 + 2;
  int new_str_sz = ceil_power2(approx_new_sz);
  char *str_wnl = new char[new_str_sz];
  std::memset(str_wnl, '\0', new_str_sz);
  add_char_every(message, str_wnl, 64, '\n'); /* encoded compressed string */
  printf("[DEBUG][%s] Info on message manipulation: Size allocated for newline "
         "augmentation: %d (traceback: %s)\n",
         date_str(buf), new_str_sz, __func__);

  // decode message from base64
  /* allocate decoded */
  char *decoded = new char[std::strlen(str_wnl) + 1];
  decoded = unbase64(str_wnl, decoded); /* decoded from base64 */
  printf("[DEBUG][%s] Info on message manipulation: Size allocated for base64 "
         "decoding: %lu (traceback: %s)\n",
         date_str(buf), std::strlen(str_wnl) + 1, __func__);

  // decompress from bzip2
  char *str_message = str_buffer_long;
  unsigned int str_message_length = 0;
  uncompress_bz2_string(decoded, str_message,
                        str_message_length); /* decoded and uncompressed */

  // Find location of first '='. then step back 8 characters. This is the
  // true start of the string
  int error = 0;
  char *nstart = std::strchr(str_message, '=');
  if (!nstart || (nstart - str_message >= 8)) {
    fprintf(stderr,
            "[ERROR][%s] failed to decode/decompress Aristarchos message!"
            "failed to find \'=\' sign in msg! (traceback: %s)\n",
            date_str(buf), __func__);
    error = 1;
  }
  nstart -= 8;

  // add newlines after every 80 chars
  if (!error) {
    approx_new_sz = std::strlen(nstart);
    approx_new_sz += approx_new_sz/80 + 2;
    if (new_str_sz < approx_new_sz) {
      new_str_sz = ceil_power2(approx_new_sz);
      delete[] str_wnl;
      str_wnl = new char[new_str_sz];
      printf("[DEBUG][%s] Note that we re-allocated memmory for adding newline characters in the 80y part case (traceback: %s)\n", date_str(buf), __func__);
    }
    std::memset(str_wnl, '\0', new_str_sz);
    add_char_every(nstart, str_wnl, 80, '\n');
  }

  // deallocate memory
  delete[] str_wnl;
  delete[] decoded;

  return error ? nullptr : nstart;
}

/// @brief Decompress a bzip2 string
/// The function accepts a bzip2 input string and decompresses it into the
/// the provided dest buffer. We can't know beforehand the size of the
/// resulting, decompressed string, so this buffer should be large enough (just
/// how larger a have no clue!). The actual size of the resulting string will be
/// written in destLen.
/// To make sure that the size of dest is large enough, the function expects
/// that it is of size ARISTARCHOS_DECODE_BUFFER_SIZE
/// @param[in] source The input bzip2 compressed string
/// @param[out] dest On success, it will hold the (null-terminated) resulting/
///             decompressed string
/// @param[out] destLen The size of the dest string, aka the size of the
///             decompressed string
/// @return On sucess, a pointer to dest will be returned; on error, the
///         function will return a null pointer
/// @see http://linux.math.tifr.res.in/manuals/html/manual_3.html
char *uncompress_bz2_string(char *source, char *dest,
                            unsigned int &destLen) noexcept {

  std::memset(dest, '\0', ARISTARCHOS_DECODE_BUFFER_SIZE);
  unsigned int sourceLen = std::strlen(source);
  int error;

  if (error =
          BZ2_bzBuffToBuffDecompress(dest, &destLen, source, sourceLen, 0, 0);
      error == BZ_OK) {
    dest[destLen] = '\0';
    return dest;
  }

  char buf[32];
  switch (error) {
  case BZ_CONFIG_ERROR:
    fprintf(stderr,
            "[ERROR][%s] decompression error: bzlib library has been "
            "mis-compiled! (traceback: %s)\n",
            date_str(buf), __func__);
    break;
  case BZ_PARAM_ERROR:
    fprintf(stderr,
            "[ERROR][%s] descompression error: dest is NULL or destLen is "
            "NULL! (traceback: %s)\n",
            date_str(buf), __func__);
    break;
  case BZ_MEM_ERROR:
    fprintf(stderr,
            "[ERROR][%s] descompression error: insufficient memory is "
            "available! (traceback: %s)\n",
            date_str(buf), __func__);
    break;
  case BZ_OUTBUFF_FULL:
    fprintf(stderr,
            "[ERROR][%s] the size of the compressed data exceeds *destLen! "
            "(traceback: %s)\n",
            date_str(buf), __func__);
    break;
  case BZ_DATA_ERROR:
    fprintf(stderr,
            "[ERROR][%s] descompression error: a data integrity error was "
            "detected in the compressed data! (traceback: %s)\n",
            date_str(buf), __func__);
    break;
  case BZ_DATA_ERROR_MAGIC:
    fprintf(stderr,
            "[ERROR][%s] descompression error: the compressed data doesn't "
            "begin with the right magic bytes! (traceback: %s)\n",
            date_str(buf), __func__);
    break;
  case BZ_UNEXPECTED_EOF:
    fprintf(stderr,
            "[ERROR][%s] descompression error: the compressed data ends "
            "unexpectedly! (traceback: %s)\n",
            date_str(buf), __func__);
    break;
  default:
    fprintf(stderr,
            "[ERROR][%s] descompression error: undocumented error! (traceback: "
            "%s)\n",
            date_str(buf), __func__);
  }

  return nullptr;
}

/// @brief Establish a connection and communicate with Aristarchos
/// This function will create a client socket and binf it to Aristarchos. Then
/// it will send to Aristarchos the command provided in the request buffer. If
/// needed, it will tray to get back Aristarchos's reply and store it in the
/// reply buffer.
/// @param[in] delay_sec Wait/stall this number of seconds after sending the
///            request and before accepting the replay
/// @param[in] request A buffer holding the command/request to send to
///            Aristarchos
/// @param[in] need_reply Set to TRUE if we must expect a reply from Aristarchos
///            (following the sending of the command/request). If set to FALSE,
///            we are only sending the request and then returning;
/// @param[out] reply If we are expecting an answer from Aristarchos (aka
///             need_reply is TRUE), this buffer will hold the reply. It must be
///             of size ARISTARCHOS_SOCKET_BUFFER_SIZE
/// @return Anything other than 0 denotes an error
int send_aristarchos_request(int delay_sec, const char *request, int need_reply,
                             char *reply) noexcept {
  char buf[32]; /* for datetime string */
  int error = 0;

  try {
    /* create and connect .... */
    printf("[DEBUG][%s] Trying to connect to Aristarchos ...\n", date_str(buf));
    ClientSocket client_socket(ARISTARCHOS_IP, ARISTARCHOS_PORT);
    printf("[DEBUG][%s] Connection with Aristarchos established!\n",
           date_str(buf));

    /* send request */
    error = client_socket.send(request);
    if (error < 0) {
      fprintf(
          stderr,
          "[ERROR][%s] Failed sending command to Aristarchos (traceback: %s)\n",
          date_str(buf), __func__);
    }

    /* if we need to, get the reply */
    if (need_reply) {
      std::memset(reply, '\0', ARISTARCHOS_SOCKET_BUFFER_SIZE);
      /* need this delay for the telescope. Could probably be shorter */
      std::this_thread::sleep_for(std::chrono::seconds{delay_sec});
      error = client_socket.recv(reply, ARISTARCHOS_SOCKET_BUFFER_SIZE);
      if (error < 0) {
        fprintf(stderr,
                "[ERROR][%s] Failed receiving command from Aristarchos "
                "(traceback: %s)\n",
                date_str(buf), __func__);
      }
    }

  } catch (std::exception &e) {
    fprintf(stderr,
            "[ERROR][%s] Exception caught while connecting to Aristarchos "
            "(traceback: %s)\n",
            date_str(buf), __func__);
    fprintf(stderr,
            "[ERROR][%s] Cannot connect to Aristarchos! (traceback: %s)\n", buf,
            __func__);
    error = 1;
  }

  return error;
}

/// @brief Translate a request to an ARISTARCHOS command
/// The translation is performed in the following way:
/// * request: grabHeader -> command: 0003RD;
/// * request callExpStart -> command: 0006RE ON;
/// * request callExpStop -> command: 0006RE OF;
/// * request: getHeaderStatus -> command: 0003RS;
/// @param[in] request A string defining the request to be translated
/// @param[out] command The translated string if the reuest is valid; should
///            be of size ARISTARCHOS_COMMAND_MAX_XHARS
/// @return A pointer to the command string if the translation was successeful;
///         else, a pointer to null
char *generate_request_string(const char *request, char *command) noexcept {

  char buf[32]; /* for datetime string */
  int status = 0;
  std::memset(command, '\0', ARISTARCHOS_COMMAND_MAX_CHARS);

  if (!std::strcmp(request, "grabHeader")) {
    std::strcpy(command, "0003RD;");
  } else if (!std::strcmp(request, "callExpStart")) {
    std::strcpy(command, "0006RE ON;");
  } else if (!std::strcmp(request, "callExpStop")) {
    std::strcpy(command, "0006RE OF;");
  } else if (!std::strcmp(request, "getHeaderStatus")) {
    std::strcpy(command, "0003RS;");
  } else {
    status = 1;
  }

  if (!status) {
    printf("[DEBUG][%s] Command to send to ARISTARCHOS: \"%s\" (translated "
           "from: \"%s\")\n",
           date_str(buf), command, request);
    return command;
  }

  fprintf(stderr,
          "[ERROR][%s] Failed to translate request: \"%s\" to an ARISTARCHOS "
          "command! (traceback: %s)\n",
          date_str(buf), request, __func__);
  return nullptr;
}
