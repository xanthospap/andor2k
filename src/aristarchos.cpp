#include "aristarchos.hpp"
#include "andor2k.hpp"
#include "cbase64.hpp"
#include "cpp_socket.hpp"
#include <bzlib.h>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <thread>

using andor2k::ClientSocket;
using andor2k::Socket;

/// @brief Buffer length for decompressing (bzip2) the FCC header
constexpr int BZ2_BUFFER_SIZE = 16384;

/// @brief for ceil_power2 we need the following to hold:
static_assert(sizeof(unsigned int) == 4);

/// @brief Structure to hold a command to be sent to Aristarchos
struct AristrachosCommand {
  // the actual command, e.g. "0006RE ON;"
  char command[64];
  // wait for reply (0-> do not wat, 1->wait, 2->optional wait, aka wait with
  // timeout)
  int8_t wait_reply = 0;
  // sleep after the command is sent (before getting the reply if any)
  int8_t sleep_after = 0;
};

bool response_has_error(const char *response) noexcept {
  return *response && (response[4] == '?' || response[5] == '?');
}

/// @brief Send a command sequence (aka an array of AristrachosCommand) to
/// FCC to get back the FITS headers buffer (encoded and compressed)
/// @param[in] num_tries Number of tries to get back a response from FCC; note
///            that if FCC responds, it is not certain that this response is
///            indeed a valid header collection; to check that, you will need
///            to decode/decompres. However, this function will count any
///            response it gets back as successeful.
/// @param[in] header FCC reply will be stored in the header buffer, which
///            should be at least ARISTARCHOS_MAX_HEADER_SIZE
/// @param[in] reply_timeout If any of the command to be sent included waiting
///            for an optional reply, this is the value of the timeout in
///            seconds
char *send_request_header_sequence(int max_tries, char *header,
                                   int reply_timeout) noexcept {
  char dbuf[32]; // for reporting datetime

  AristrachosCommand cmd_sequence[4];
  std::strcpy(cmd_sequence[0].command, "0006RE ON;");
  cmd_sequence[0].wait_reply = 0;
  cmd_sequence[0].sleep_after = 8;
  std::strcpy(cmd_sequence[1].command, "0003RS;");
  cmd_sequence[1].wait_reply = 1;
  cmd_sequence[1].sleep_after = 2;
  std::strcpy(cmd_sequence[2].command, "0006RE OF;");
  cmd_sequence[2].wait_reply = 2; // optional reply
  cmd_sequence[2].sleep_after = 2;
  std::strcpy(cmd_sequence[3].command, "0003RD;");
  cmd_sequence[3].wait_reply = 1;
  cmd_sequence[3].sleep_after = 2;

  int count = 0; // current communication try
  int error = 1; // communication error

  // open socket and exchange messages
  while (count < max_tries && error) {

    error = 0;

    try {
      ++count;

      // open a client socket to communicate with FCC
      ClientSocket client_socket(ARISTARCHOS_IP, ARISTARCHOS_PORT);

      // set the socket to be non-blocking, set time-out
      struct timeval tv;
      tv.tv_sec = reply_timeout;
      tv.tv_usec = 0;
      setsockopt(client_socket.sockid(), SOL_SOCKET, SO_RCVTIMEO,
                 (const char *)&tv, sizeof tv);

      printf("[DEBUG][%s] Connection to FCC at %s:%d!\n", date_str(dbuf),
             ARISTARCHOS_IP, ARISTARCHOS_PORT);

      // sending/receiving commands/replies via the header buffer
      for (int i = 0; i < 4; i++) {

        // send command
        std::strcpy(header, cmd_sequence[i].command);
        int bt_sent = client_socket.send(header);

        // if sending fails, close socket and retry (if allowed)
        if (bt_sent <= 0) {
          fprintf(stderr,
                  "[ERROR][%s] Failed to transmit message to FCC! Try: %d/%d, "
                  "message: [%s] (traceback: %s)\n",
                  date_str(dbuf), count, max_tries, header, __func__);
          fprintf(stderr,
                  "[ERROR][%s] Aborting connection and starting over! "
                  "(traceback: %s)\n",
                  dbuf, __func__);
          error = 1;
          client_socket.close_socket();
          break;
        } else {
          printf("[DEBUG][%s] Command sent to server [%s]\n", date_str(dbuf),
                 header);
        }

        // sleep if needed before getting reply
        std::this_thread::sleep_for(
            std::chrono::seconds(cmd_sequence[i].sleep_after));

        // if we need a reply, get it (remmber, we have a time-out for
        // responses)
        if (cmd_sequence[i].wait_reply) {

          int timeout_set = 0; // timeout set while waiting for reply

          // get reply
          std::memset(header, '\0', ARISTARCHOS_MAX_HEADER_SIZE);
          int bt_recv = client_socket.recv(header, ARISTARCHOS_MAX_HEADER_SIZE);

          if (bt_recv <= 0) {
            // check for timeout ...
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
              fprintf(stderr,
                      "[ERROR][%s] Failed to get answer from server, timeout "
                      "reached!; request was: [%s] (traceback: %s)\n",
                      date_str(dbuf), cmd_sequence[i].command, __func__);
              errno = 0;
              timeout_set = 1;
            } else {
              fprintf(stderr,
                      "[ERROR][%s] Failed to get answer from server; request "
                      "was: [%s] (traceback: %s)\n",
                      date_str(dbuf), cmd_sequence[i].command, __func__);
            }
            // only an error if the reply is absolutelly needed
            if (timeout_set && cmd_sequence[i].wait_reply > 1) {
              printf("[DEBUG][%s] Time-out while wating for reply but going "
                     "on; reply not demanded!\n",
                     date_str(dbuf));
            } else {
              fprintf(stderr,
                      "[ERROR][%s] Aborting connection and starting over! "
                      "(traceback: %s)\n",
                      date_str(dbuf), __func__);
              error = 1;
              client_socket.close_socket();
              break;
            }
            // we got back a response
          } else {
            printf("[DEBUG][%s] Here is the server response (%dbytes) [%s]\n",
                   date_str(dbuf), bt_recv, header);
            if (response_has_error(header)) {
              fprintf(stderr,
                      "[ERROR][%s] Seems like the response signaled an error! "
                      "(traceback: %s)\n",
                      date_str(dbuf), __func__);
              fprintf(stderr,
                      "[ERROR][%s] Aborting connection and starting over! "
                      "(traceback: %s)\n",
                      dbuf, __func__);
              error = 1;
              client_socket.close_socket();
              break;
            }
          }

          // no reply needed, continue with next command
        } else {
          printf("[DEBUG][%s] No reply needed, continuing ...\n",
                 date_str(dbuf));
        } // end reply handling for current command

      } // sent all commands, got all replies (for current try) ...
      client_socket.close_socket();
      // fixme - set error to 0, to mark a successeful try
      error = 0;

    } catch (std::exception &e) {
      fprintf(stderr,
              "[ERROR][%s] Failed to open client socket for FCC at %s:%d "
              "(traceback: %s)\n",
              date_str(dbuf), ARISTARCHOS_IP, ARISTARCHOS_PORT, __func__);
    }

    // sleep a bita before re-trying .... do i need this?
    std::this_thread::sleep_for(std::chrono::milliseconds(200));
  }

  if (error)
    return nullptr;
  return header;
}

int get_aristarchos_headers(int num_tries,
                            std::vector<FitsHeader> &headers) noexcept {
  char buf[32];  // for datetime reporting
  int error = 1; // error

  // clear everything from headers vector
  if (!headers.empty())
    headers.clear();

  char raw_msg[ARISTARCHOS_MAX_HEADER_SIZE];
  char ascii_buf[BZ2_BUFFER_SIZE];
  int ctry = 0;              // current try for headers
  char *ascii_str = nullptr; // header ascii string
  unsigned ascii_len;        // size of resulting, ascii header buffer

  // try to get a header buffer from FCC
  while (ctry < num_tries && error) {
    ++ctry;
    printf("[DEBUG][%s] Trying to get Aristarchos headers (try %d/%d)\n",
           date_str(buf), ctry, num_tries);

    // start off with no errors
    error = 0;

    // send header request (opne socket to FCC and send request command
    // sequence). If we do get something back, check if it can be resolved to
    // a valid header string
    std::memset(raw_msg, 0, ARISTARCHOS_MAX_HEADER_SIZE);
    char *raw_msg_p = send_request_header_sequence(num_tries, raw_msg, 2);

    // did we get anything back?
    if (raw_msg_p == nullptr) {
      fprintf(stderr,
              "[ERROR][%s] Failed getting headers from FCC@%s:%d try %d/%d "
              "(traceback: %s)\n",
              date_str(buf), ARISTARCHOS_IP, ARISTARCHOS_PORT, ctry, num_tries,
              __func__);
      error = 1;
      continue;
    }

    // we got something back! Could be the bziped, ubase64 encoded headers,
    // but we need to turn the buffer to ascii to check this
    printf("[DEBUG][%s] Got headers from FCC; now trying to decode them\n",
           date_str(buf));

    // decode/decompress the reponse and store result in the ascii_buf buffer.
    // the size of the (resulting) ascii string (hopefully the headers with no
    // line breaks) is ascii_len
    ascii_str =
        decode_message(raw_msg_p, ascii_buf, BZ2_BUFFER_SIZE, ascii_len);
    if (ascii_str == nullptr) {
      fprintf(stderr,
              "[ERROR][%s] Failed decoding/decompressing headers, try %d/%d "
              "(traceback: %s)\n",
              date_str(buf), ctry, num_tries, __func__);
      error = 1;
      continue;
    }
  } // keep trying for FCC headers ...

  // we either got the headers, in which case error==0, or we didn't, in which
  // case error==1
  if (error) {
    fprintf(stderr,
            "[ERROR][%s] Failed getting header buffer from FCC! Maximum number "
            "of tries (%d) reached (traceback: %s)\n",
            date_str(buf), num_tries, __func__);
    return 1;
  }

  // if we decoded the headers, extract them to a vector. Note that the
  // decoded, ascii buffer (ascii_buf) holds the headers as a aingle string,
  // aka with no newline characters!
  printf("[DEBUG][%s] Splitting decoded headers to match FITS header format\n",
         date_str(buf));
  error = decoded_str_to_header(ascii_str, ascii_len - 1, headers);
  if (error) {
    fprintf(stderr,
            "[ERROR][%s] Failed to translate decoded headers to FITS "
            "format!\n",
            date_str(buf));
    return error;
  }

  // All done! return success
  printf("[DEBUG][%s] Actual number of headers decoded is :%d\n", date_str(buf),
         (int)headers.size());

  return error;
}

/// @brief Add char after every n characters in string
/// This function will create a new copy of the string source, where after
/// every every character an extra delim character is added. A delim
/// character is also added at the end of the string. The new string is null
/// terminated. Example (every=6, delim='-'): Original string [ab] of size 2,
/// becomes: [ab+] Original string [abcdef] of size 6, becomes: [abcdef+]
/// Original string [abcdefg] of size 7, becomes: [abcdef+g+]
/// Original string [abcdefghijklmnopqrstuvwxy] of size 25, becomes:
/// [abcdef+ghijkl+mnopqr+stuvwx+y+]
/// @param[in] source the original, null-terminated string
/// @param[out] dest the resulting string; the size of the array must be
/// large
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

/// @brief Decode a message to plain ascii string.
/// The function will try to decode the input message, following two steps:
/// * decode from base64
/// * uncompress from bzip2 (to plain ascii)
/// @param[in] raw_message The encypted/compressed message to decode
/// @param[in] ascii_str The resulting plain ascii string is stored in this
///            buffer
/// @param[in] buff_len Size of the ascii_str buffer (needed by the
///            decompression function)
/// @param[out] ascii_len actuall length of the resulting, ascii string
char *decode_message(char *raw_message, char *ascii_str, int buff_len,
                     unsigned &ascii_len) noexcept {

  char buf[32]; // for datetime string

  printf("[DEBUG][%s] Started decoding message got from Aristarchos "
         "(traceback: %s)\n",
         date_str(buf), __func__);

  // Find the start of the block. This is usually BF=[B64....];
  char *start = std::strstr(raw_message, "BF=");
  if (!start) {
    fprintf(stderr,
            "[ERROR][%s] Failed to decode message; could not find start of "
            "block \"B64\" (traceback: %s)\n",
            date_str(buf), __func__);
    return nullptr;
  }
  start += 3;

  // find the end of the essage, which should be the ';' character, and
  // replace it with a null-terminating character
  char *end = start;
  while (*end && *end != ';')
    ++end;
  if (*end != ';') {
    fprintf(
        stderr,
        "[ERROR][%s] Failed to decode message; could not find end of message "
        "aka \";\" (traceback: %s)\n",
        date_str(buf), __func__);
    return nullptr;
  }
  *end = '\0';

  // compute the length of the base64-decoded string and allocate a buffer to
  // hold the result (the decrypted message)
  int len = base64decode_len(start);
  char *ub64_buf = new char[len];
  std::memset(ub64_buf, 0, len);

  // decrypt ...
  int bts = base64decode(ub64_buf, start);
  if (bts != len - 1) {
    fprintf(stderr,
            "[ERROR][%s] Failed to decode message; base64 decoding failed! "
            "(traceback: %s)\n",
            date_str(buf), __func__);
    delete[] ub64_buf;
    return nullptr;
  }

  // nice ... the decrypted message is now stored in ub64_buf with a size of
  // bts plus 1 for the null-terminating character

  // decompress from bzip2 (after the call, ascii_len holds the size of the
  // output string; before the call, it should hold the size of the input
  // buffer)
  ascii_len = buff_len;
  int error =
      BZ2_bzBuffToBuffDecompress(ascii_str, &ascii_len, ub64_buf, bts, 1, 1);
  if (error != BZ_OK) { // report error details and return
    fprintf(stderr, "ERROR Failed to decompress data (bz2)!\n");
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
      fprintf(
          stderr,
          "[ERROR][%s] descompression error: undocumented error! (traceback: "
          "%s)\n",
          date_str(buf), __func__);
    }

    delete[] ub64_buf;
    return nullptr;
  }

  // clear memory for the decrypted message
  delete[] ub64_buf;

  // return
  return ascii_str;
}

/// @brief Given a plain FITS header buffer (ascii string with no newlines),
/// parse it to valid FitsHeader instances
/// Note that the function assumes:
/// 1. Each header line has a size of 80 characters
/// 2. Each header line has and '=' sign at the 8th place
/// @param[in] decoded_msg An ascii FITS header buffer with no newlines (this
///            is meant to be the decoded response from FCC, after an 0003RD;
///            command is issued)
/// @param[in] msg_len Length of the input ascii message/buffer (no newline
///            character
/// @param[out] header_vec A vector of FitsHeader to hold the parsed headers
///            from the input buffer
int decoded_str_to_header(const char *decoded_msg, unsigned msg_len,
                          std::vector<FitsHeader> &header_vec) noexcept {
  constexpr int header_size = 80;

  char buf[32]; // for datetime string
  // int decoded_msg_sz = std::strlen(decoded_msg);

  // clear vector and allocate capacity
  header_vec.clear();
  if (header_vec.capacity() < 100)
    header_vec.reserve(100);

  // While the stream is full i.e. no EOF
  int max_hdrs = 1000, hdr_count = 0;
  int error = 0;
  const char *start = decoded_msg;
  const char *end;
  while (*start && (!error && hdr_count < max_hdrs)) {

    if (start - decoded_msg > msg_len) {
#ifdef DEBUG
      printf("[DEBUG][%s] Parsing ended after %d characters "
             "(traceback: %s)\n",
             date_str(buf), (int)(start - decoded_msg), __func__);
#endif
      break;
    }
    end = start + header_size;

    // string to consider is [start,end)
#ifdef DEBUG
    printf("[DEBUG][%s] Parsing new header line: [%.*s] of size: %d "
           "(traceback: %s)\n",
           date_str(buf), header_size - 1, start, header_size - 1, __func__);
#endif

    bool is_header_line = true;

    if (*(start + 8) != '=' && is_header_line) {
      fprintf(stderr,
              "[WRNNG][%s] Expected \'=\' at 8th place, probably not a header "
              "line; line skipped (traceback: %s)\n",
              date_str(buf), __func__);
      is_header_line = false;
    }

    if (is_header_line) {
      FitsHeader hdr;
      hdr.type = FitsHeader::ValueType::tchar32;

      // Keyword is the first 8 characters
      std::memset(hdr.key, '\0', FITS_HEADER_KEYNAME_CHARS);
      std::strncpy(hdr.key, start, 8);

      // Value is the next batch up until the '/' character
      std::memset(hdr.cval, '\0', FITS_HEADER_VALUE_CHARS);
      const char *vstop = std::strchr(start + 8, '/');
      if (vstop == nullptr) {
        fprintf(stderr,
                "[ERROR][%s] Failed to resolve header line! could not find "
                "start of comment character. (traceback: %s)\n",
                date_str(buf), __func__);
        return -1;
      }
      std::strncpy(hdr.cval, start + 11, vstop - (start + 11));

      // The comment is the remainder
      int remainder_sz = header_size - (vstop - start) - 1;
      std::memset(hdr.comment, '\0', FITS_HEADER_COMMENT_CHARS);
      std::strncpy(hdr.comment, vstop + 1, remainder_sz);

#ifdef DEBUG
      printf("[DEBUG][%s] Resolved Aristarchos header line: \n\tkey:[%s]"
             "\n\tvalue:[%s]\n\tcomment:[%s]\n",
             date_str(buf), hdr.key, hdr.cval, hdr.comment);
#endif

      // push back the new resolved header
      header_vec.emplace_back(hdr);
    }

    start = end;
  }

  return 0;
}
