#include "cpp_socket.hpp"
#include "andor2kd.hpp"
#include <chrono>
#include <cstring>

class AcquisitionSeriesReporter {
public:
  AcquisitionSeriesReporter(const andor2k::Socket *s, long exp_msec, int n_images,
                 const std::chrono::system_clock::time_point& s_start) noexcept;
  void report() noexcept;
private:
  const andor2k::Socket *socket;
  long exposure_millisec;
  std::chrono::system_clock::time_point series_start; // start of series
  int num_images;
  long every_millisec;
  int len_const_prt;
  char mbuf[MAX_SOCKET_BUFFER_SIZE];

  void clear_buf() noexcept { std::memset(mbuf, 0, MAX_SOCKET_BUFFER_SIZE); }
}; // AcquisitionSeriesReporter
