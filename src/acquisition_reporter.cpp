#include "acquisition_reporter.hpp"
#include "andor2k.hpp"
#include "andor2kd.hpp"
#include "cpp_socket.hpp"
#include <chrono>
#include <cstring>
#include <mutex>
#include <thread>

using std_time_point = std::chrono::system_clock::time_point;

extern std::mutex g_mtx;
extern int abort_set;

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

/// A class to handle reporting while an ANDOR2K image acquisition takes place.
/// The only purpose of this class, is to call the report function (in a
/// different thatn the one waiting on the acquisition) and report the progress
/// status of the exposure.
/// @param[in] s A pointer to an (already opened) socket; the report function
///              will regurarly send reports to this scoket (but will receive no
///              incoming messages)
/// @param[in] exp_msec The exposure time of the image in milliseconds of the
///              image; note that this should be the actual exposure time, which
///              could be different than the one specified by the user. See the
///              GetAcquisitionTimings API function.
/// @param[in] s_start The start of the acquisition time, as a time_point. This
///              time should be after the StartAcquisition() call and before
///              the WaitForAcquisition().
AcquisitionReporter::AcquisitionReporter(const andor2k::Socket *s,
                                         long exp_msec,
                                         const std_time_point &s_start) noexcept
    : socket(s), exposure_ms(exp_msec), series_start(s_start),
      every_ms(exp2tick_every(exp_msec)) {
  // write constant part of message so that we don't have to write it every
  // time
  clear_buf();
  len_const_prt = std::sprintf(
      mbuf, "info:Acquiring image ...;status:Acquiring;image 1/1;time:");
}

/// This function will constantly report to the instance's socket its current
/// state, untill it can get a hold of the g_mtx (global) mutex. Reporting is
/// performed in an every_ms millisecond interval.
void AcquisitionReporter::report() noexcept {
  int it = 0;
#ifdef DEBUG
  char dbuf[64];
#endif

  // get a lock instance but do not try to lock yet!
  std::unique_lock<std::mutex> lk(g_mtx, std::defer_lock);

  // if we are getting a bias image (aka exposure time is 0), wrap up and exit
  // (aka) just send one message to client, that we are done
  if (!exposure_ms) {
#ifdef DEBUG
    printf("[DEBUG][%s] image is bias! not using locks (%s)!\n", date_str(dbuf),
           __func__);
#endif
    date_str(mbuf + len_const_prt);
    std::sprintf(mbuf + std::strlen(mbuf),
                 "done;progperc:%d;sprogperc:%d;elapsedt:%.2f;selapsedt:%.2f",
                 100, 100, 0e0, 0e0);
    return;
  }

  long from_series_start = 0;
  int image_done = 0, series_done = 0;

  // report while we can't get a hold of the lock
  while (!lk.try_lock()) {
    // time now
    auto t_now = std::chrono::high_resolution_clock::now();

    // time since started this exposure
    from_series_start = std::chrono::duration_cast<std::chrono::milliseconds>(
                            t_now - series_start)
                            .count();

    // percentage of exposure finished
    image_done = (from_series_start * 100 / exposure_ms);

    // percentage of series finished
    series_done = image_done; //(from_acquisition_start * 100 / total_ms);

    // prepare message to be sent (add datetime and information)
    date_str(mbuf + len_const_prt);
    std::sprintf(mbuf + std::strlen(mbuf),
                 ";progperc:%d;sprogperc:%d;elapsedt:%.2f;selapsedt:%.2f",
                 image_done, series_done, from_series_start / 1e3,
                 from_series_start / 1e3);

    // send message to client via the socket
    socket->send(mbuf);

    // sleep a bit ...
    std::this_thread::sleep_for(std::chrono::milliseconds(every_ms));
    ++it;
  }

  // we got a lock! that means that the (current) exposure/acquisition is
  // over
  lk.unlock();

  // prepare last message to be sent (add datetime and indormation). we are
  // going to pretend that the acquisition is done 100% except if the (global)
  // variable abort_set is non-zero, in which case probably the acquisition
  // was aborted
  if (!abort_set) {
    image_done = 100;
  } else {
    image_done = (from_series_start * 100 / exposure_ms);
  }
  series_done = image_done;
  date_str(mbuf + len_const_prt);
  std::sprintf(mbuf + std::strlen(mbuf),
               ";progperc:%d;sprogperc:%d;elapsedt:%.2f;selapsedt:%.2f",
               image_done, series_done, from_series_start / 1e3,
               from_series_start / 1e3);

  // all done
  return;
}
