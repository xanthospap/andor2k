#include "ar_fits_header.cpp"
#include <bzlib.h>
  
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
}
