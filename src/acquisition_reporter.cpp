#include "acquisition_reporter.hpp"
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

int exp2tick_every(long iexp) noexcept {
  long min_tick = static_cast<long>(0.5e0 * 1e3); //  500 millisec
  long max_tick = static_cast<long>(5 * 1e3);     // 5000 millisec
  if (iexp < min_tick) {
    return iexp;
  } else if (iexp < 2000) {
    return iexp / 2L;
  } else if (iexp < 5000) {
    return iexp / 3L;
  } else if (iexp < 10000) {
    return iexp / 4L;
  } else if (iexp < 20000) {
    return iexp / 6L;
  } else if (iexp < 60000) {
    return iexp / 10L;
  } else if (iexp < 120000) {
    return iexp / 15L;
  } else if (iexp < 5 * 60000) {
    return iexp / 20L;
  } else {
    int nr = iexp / max_tick;
    return iexp / nr;
  }
}

AcquisitionReporter::AcquisitionReporter(const Socket *s, long exp_msec,
                 const std_time_point &s_start) noexcept :
    socket(s),
    exposure_ms(exp_msec),
    series_start(s_start),
    every_ms(exp2tick_every(exp_msec))
  {
    // write constant part of message so that we don't have to write it every
    // time
    clear_buf();
    len_const_prt = std::sprintf(
        mbuf, "info:Acquiring image ...;status:Acquiring;image 1/1;time:");
  }

  // constantly report untill we can get a lock of the mutex
  void AcquisitionReporter::report() noexcept {
    int it = 0;

    // time when function started
    // auto t_start = std::chrono::high_resolution_clock::now();

    // get a lock instance but do not try to lock yet!
    std::unique_lock<std::mutex> lk(g_mtx, std::defer_lock);

    // report while we can't get a hold of the lock
    long from_thread_start=0; // , from_acquisition_start=0;
    int image_done=0, series_done=0;
    while (!lk.try_lock()) {
      // time now
      auto t_now = std::chrono::high_resolution_clock::now();
      
      // time since started this exposure
      from_thread_start =
          std::chrono::duration_cast<std::chrono::milliseconds>(t_now - series_start)
              .count();
      
      //// time since start of exposure series
      //from_acquisition_start =
      //    std::chrono::duration_cast<std::chrono::milliseconds>(t_now -
      //                                                          series_start)
      //        .count();
      
      // percentage of exposure finished
      image_done = (from_thread_start * 100 / exposure_ms);
      
      // percentage of series finished
      series_done = image_done; //(from_acquisition_start * 100 / total_ms);
      
      // prepare message to be sent (add datetime and indormation)
      date_str(mbuf + len_const_prt);
      std::sprintf(mbuf + std::strlen(mbuf),
                   ";progperc:%d;sprogperc:%d;elapsedt:%.2f;selapsedt:%.2f",
                   image_done, series_done, from_thread_start / 1e3,
                   from_thread_start / 1e3);
      
      // send message to client via the buffer
      socket->send(mbuf);
      
      // sleep a bit ...
      std::this_thread::sleep_for(std::chrono::milliseconds(every_ms));
      ++it;
    }

    // we got a lock! that means that the (current) exposure/acquisition is 
    // over
    lk.unlock();

    // prepare last message to be sent (add datetime and indormation)
    date_str(mbuf + len_const_prt);
    std::sprintf(mbuf + std::strlen(mbuf),
                 ";progperc:%d;sprogperc:%d;elapsedt:%.2f;selapsedt:%.2f",
                 100, series_done, from_thread_start / 1e3,
                 from_thread_start / 1e3);
    
    // all done
    return;
  }
