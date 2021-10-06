#include "cpp_socket.hpp"
#include "andor2k.hpp"
#include "andor2kd.hpp"
#include <chrono>
#include <cstring>
#include <mutex>
#include <thread>

using andor2k::Socket;
using std_time_point = std::chrono::system_clock::time_point;

extern std::mutex g_mtx;
extern int cur_img_in_series;

class AcquisitionSeriesReporter {
public:
  // constantly report untill we can get a lock of the mutex
  void report() noexcept {
    // time when function started
    auto t_start = std::chrono::high_resolution_clock::now();

    // get a lock instance but do not try to lock yet!
    std::unique_lock<std::mutex> lk(g_mtx, std::defer_lock);

    int curimg = cur_img_in_series;
    long from_thread_start=0, from_acquisition_start=0;
    int image_done=0, series_done=0;
    while (!lk.try_lock()) {
      // time now
      auto t_now = std::chrono::high_resolution_clock::now();
      
      // time since started this exposure
      from_thread_start =
          std::chrono::duration_cast<std::chrono::milliseconds>(t_now - t_start)
              .count();
      
      // time since start of exposure series
      from_acquisition_start =
          std::chrono::duration_cast<std::chrono::milliseconds>(t_now -
                                                                series_start)
              .count();
      // percentage of exposure finished
      image_done = (from_thread_start * 100 / exposure_ms);
      
      // percentage of series finished
      series_done = (from_acquisition_start * 100 / total_ms);
      
      // prepare message to be sent (add datetime and indormation)
      date_str(mbuf + len_const_prt);
      std::sprintf(mbuf + std::strlen(mbuf),
                   ";progperc:%d;sprogperc:%d;elapsedt:%.2f;selapsedt:%.2f",
                   image_done, series_done, from_thread_start / 1e3,
                   from_acquisition_start / 1e3);
      // send message to client via the buffer
      socket->send(mbuf);
      
      // sleep a bit ...
      std::this_thread::sleep_for(std::chrono::milliseconds(every_millisec));
    }

  }//report

private:
  int num_images;
  long every_millisec = 100;
  char mbuf[MAX_SOCKET_BUFFER_SIZE];

  void clear_buf() noexcept { std::memset(mbuf, 0, MAX_SOCKET_BUFFER_SIZE); }
};// AcquisitionSeriesReporter
