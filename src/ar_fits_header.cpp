#include "ar_fits_header.cpp"
#include <bzlib.h>
#ifdef USE_SSL_B64_DECODER
#include <openssl/evp.h>
#endif

#ifdef USE_SSL_B64_DECODER
unsigned char *decode64(const char *ascdata, int length, int& status) {
  status = 0;
  const auto pl = 3 * length / 4;
  auto output = reinterpret_cast<unsigned char *>(calloc(pl + 1, 1));
  const auto ol = EVP_DecodeBlock(
      output, reinterpret_cast<const unsigned char *>(ascdata), length);
  if (pl != ol)
    status = 1;
  return output;
}
#else
unsigned char* decode64(const char* ascdata, int length, int& status) {
  status = 0;
  int bits_collected = 0;
  unsigned int accumulator = 0;
  std::size_t res_sz = 3 * length / 4 + 1;
  unsigned char *result = new char[res_sz];
  std::memset(result, '\0', res_sz);

  const char* c = ascdata;
  int i=0;
  while (*c) {
    if (!(std::isspace(*c) || *c == '=')) {
      // Skip whitespace and padding. Be liberal in what you accept.
      const int x = (int)(*c);
      if ((x > 127) || (x < 0) || (reverse_table[x] > 63)) {
        status = 1;
        return result;
      }
      accumulator = (accumulator << 6) | reverse_table[x];
      bits_collected += 6;
      if (bits_collected >= 8) {
        bits_collected -= 8;
        result[i++] = static_cast<unsigned char>((accumulator >> bits_collected) & 0xffu);
      }
    }
    ++c;
  }
  return result;
}
#endif
  
int ArFitsHeader::send_request(bool reply_expected, int sleep_sec) noexcept {
  int status = 0;
  print("\nHeader request on %s:%d", m_ip, m_port);
  // Create a client socket to send request and optionaly get response
  try {
    // Construct and connect a client socket. May throw ....
    ClientSocket client_soc(m_ip, m_port);
    if (!client_soc.send(m_request_buf)) return -1;
    // Need some delay for the telescope
    std::this_thread::sleep_for(std::chrono::milliseconds(sleep_sec * 1000));
    if (reply_expected) {
      if (!client_soc.recv(m_response_buf, details::RESPONSE_BUF_SZ)) return -2;
    }
  } catch (std::exception &e) {
    std::cerr << "\n[ERROR] Exception was thrown during send/receive, while"
              << " trying to get FITS header information.\n";
    status = 1;
  }
  return status;
}
  
int ArFitsHeader::fill_request_buffer(const char *request_str) noexcept {
  if (!std : strcmp(request_str, "grabheader")) {
    std::strcpy(m_request_buf, "0003RD;");
    return 0;
  } else if (!(std::strcmp(request_str, "callExpStart")) {
    std::strcpy(m_request_buf, "0006RE ON;");
    return 0;
  } else if (!(std::strcmp(request_str, "callExpStop")) {
    std::strcpy(m_request_buf, "0006RE OF;");
    return 0;
  } else if (!(std::strcmp(request_str, "getHeaderStatus")) {
    std::strcpy(m_request_buf, "0003RS");
    return 0;
#ifdef DEBUG
  } else if (!(std::strcmp(request_str, "perlsocTest")) {
    std::strcpy(m_request_buf, "testing\n");
    return 0;
#endif
  }
  return 1;
}

/// 
/// 
int ArFitsHeader::decode_message() noexcept {
  constexpr std::size_t mbsz = 1024*1024; //TODO is this too large?
  char buf[mbsz];
  char *ub64_msg, *msg_start, *msg_end;

  // // Find the start of the block. This is usually BF=[B64....];
  if ((msg_start = std::strstr(raw_msg, "BF=")) == NULL) {
    std::cerr<<"\n[WARN]  No start of block found in header...";
    return 1;
  }
  msg_start+=3;

  // Now find the end, where the semicolon is.
  if ((msg_end = sd::strstr(raw_msg, ";")) == NULL) {
    std::cerr<<"\n[WARN]  No end of block found in header...";
    return 2;
  }

  std::size_t msg_size = msg_end - msg_start;
  // If header block is small, something is wrong
  if (msg_size < 100) {
    std::cerr<<"\n[WARN]  Telescope header is too small, something is wrong!";
    return 3;
  }
  if (msg_size > mbsz) {
    std::cerr<<"\n[WARN]  Telescope header is too large, something is wrong!";
    return 3;
  }
  
  // Add newlines after every 64 characters.
  // TODO whay do we have to do this? its the way legacy code does it
  int j=0;
  for (int i=msg_start; i<msg_end; i++) {
    if (i % 64 == 0) {
      buf[j++] = '\n';
    } else {
      buf[j++] = raw_msg[i];
      --i;
    }
  }
  buf[j++] = '\n';
  buf[j] = '\0';

  // decode msg from base64 to string
}
