#ifndef __ARISTARCHOS_ARFITSHEADER_HPP__
#define __ARISTARCHOS_ARFITSHEADER_HPP__

#include <string>

namespace aristarchos {

namespace details {
constexpr int MAX_IP_CHARS = 16;
constexpr std::size_t REQUEST_BUF_SZ = 256;
constexpr std::size_t RESPONSE_BUF_SZ = 256;
} // namespace details

class ArFitsHeader {
public:
  ArFitsHeader(const char *ip, int port, int retrys) : : m_port(port) {
    std::strncpy(m_ip, ip, details::MAX_IP_CHARS);
    int count, nheaders, error;
    // try to access the headers
    while (count < retrys && error !=)
      ;
  }

  /// @brief Generate a new request to send to the telescope.
  int new_request() noexcept {}

  /// @brief Send the request contained in the instance's m_request_buf.
  int send_request(bool reply_expected, int &status, int sleep_sec) noexcept {
    std::size_t bytes_sent = 0, bytes_recv = 0;
    status = 0;
    print("\nHeader request on %s:%d", m_ip, m_port);
    // Create a client socket to send request
    try {
      ClientSocket client_soc(m_ip, m_port); // may throw ....
      byes_sent = client_soc.send(m_request_buf);
      // Need some delay for the telescope
      std::this_thread::sleep_for(std::chrono::milliseconds(sleep_sec * 1000));
      if (reply_expected) {
        bytes_recv = client_soc.recv(m_response_buf, details::RESPONSE_BUF_SZ);
      }
    } catch (std::exception &e) {
      std::cerr << "\n[ERROR] Exception was thrown during send/receive, while"
                << " trying to get FITS header information.\n";
      status = 1;
    }
    if (!byes_sent)
      status = -1;
    if (reply_expected && !bytes_recv)
      status = -2;
#ifdef DEBUG
#endif
    return status;
  }

  /// @brief Construct the string requesting the relevant header and store it
  ///        in the instance's m_request_buf.
  /// This function will actually translate the passed-in request string (a
  /// header request) to the corresponding telescope command. The command
  /// will then be filled in the instance's m_request_buf.
  /// @param[in] The request string; for a valid list of request strings see
  ///            below.
  /// @return An integer; if not zero, then the request string could not
  ///         be translated and the m_request_buf is left unchanged.
  /// @note  * Currently, the command with the max number of chars is
  ///          "0006RE ON;" which is 10 character long (if needed, you must
  ///          also account for the null terminating char).
  ///        * Here is a list of request string and the respective commands:
  ///          Request String | Request Command
  ///          ---------------|----------------------------
  ///            'grabheader' | '0003RD;'
  ///          'callExpStart' | '0006RE ON;'
  ///           'callExpStop' | '0006RE OF;'
  ///       'getHeaderStatus' | '0003RS'
  ///           'perlsocTest' | 'testing\n'
  int fill_request_buffer(const char *request_str) noexcept {
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

private:
  char m_ip[MAX_IP_CHARS]{"127.0.0.1"};
  int m_port{5001};
  ///< Message to send to FITS server
  char m_request_buf[details::REQUEST_BUF_SZ];
  ///< Message received from FITS server
  char m_response_buf[details::RESPONSE_BUF_SZ];
  std::string rawMessage;          ///< Returned from FITS server
  std::string ub64message;         ///< Un-bae64'd message
  std::string decodedMessage;      ///< The decoded block
  std::string definealHeaderBlock; ///< The final block from the header,
                                   ///< with newlines
  int error;
}; // ArFitsHeader

} // namespace aristarchos
#endif
