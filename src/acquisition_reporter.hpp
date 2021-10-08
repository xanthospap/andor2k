#include "andor2kd.hpp"
#include "cpp_socket.hpp"
#include <chrono>
#include <cstring>

class AcquisitionReporter {
public:
  AcquisitionReporter(
      const andor2k::Socket *s, long exp_msec,
      const std::chrono::system_clock::time_point &s_start) noexcept;

  // constantly report untill we can get a lock of the mutex
  void report() noexcept;

private:
  const andor2k::Socket *socket; // socket to send message to
  long exposure_ms;              // exposure
  std::chrono::system_clock::time_point series_start; // start of series
  long every_ms; // sleeps for every_ms milliseconds, then reports
  char mbuf[MAX_SOCKET_BUFFER_SIZE]; // char buffer (for message)
  int len_const_prt; // length of the constant part of the message

  void clear_buf() noexcept { std::memset(mbuf, 0, MAX_SOCKET_BUFFER_SIZE); }
};
