#include "acquisition_series_reporter.hpp"
#include "andor2k.hpp"
#include "andor2kd.hpp"
#include "cpp_socket.hpp"
#include <chrono>
#include <cstring>
#include <mutex>
#include <thread>

using andor2k::Socket;
using std_time_point = std::chrono::system_clock::time_point;
using namespace std::chrono;

// lock for the whole acquisition/series
extern std::mutex g_mtx;

// current image in series; this is set by the main thread (only safe to read
// not write!). Note that image indexing starts at 1
extern int cur_img_in_series;

// using experimental data, it looks that the time needed to 'get' an image
// in an rta follows a simple regression pattern. But, the pattern is a little
// different for the first image is an series (than for all the rest).
long estimated_time_per_image(long exposure_millisec, int img_nr) noexcept {
  if (!img_nr) {
    double exp_in_sec = static_cast<double>(exposure_millisec) / 1e3;
    return static_cast<long>(999.677e0 * exp_in_sec + 961.827e0);
  }
  return static_cast<long>(700.729e0) + exposure_millisec;
}

// an estimate of the whole duration of the series in milliseconds
long estimated_series_time(long exposure_millisec, int num_images) noexcept {
  return estimated_time_per_image(exposure_millisec, 0) +
         estimated_time_per_image(exposure_millisec, 1) * (num_images - 1) +
         4 * num_images;
}

/// A class to handle reporting while an ANDOR2K image acquisition takes place.
/// The only purpose of this class, is to call the report function (in a
/// different thatn the one waiting on the acquisition) and report the progress
/// status of the exposure.
/// This version is reponsible for reporting while a series of acquisitions
/// takes place, e.g. when using RunTillAbort or Kinematic mode.
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
AcquisitionSeriesReporter::AcquisitionSeriesReporter(
    const Socket *s, long exp_msec, int n_images,
    const std_time_point &s_start) noexcept
    : socket(s), exposure_millisec(exp_msec), series_start_t(s_start),
      num_images(n_images), every_millisec(200) {
  // write constant part of message so that we don't have to write it every
  // time
  clear_buf();
  len_const_prt = std::sprintf(mbuf, "info:Acquiring image series...;time:");
}

/// Dead simple ration class to help with adjust_timing function
struct Ratio {
  long nom, denom;
  constexpr long operator*(long l) noexcept { return l * nom / denom; }
};

/// find the largest ratio of every_ms that we can subtract from
/// from_image_start so that it is not negative
inline long adjust_timing(long from_image_start, long every_ms) noexcept {
  static Ratio fractions[] = { {3, 4}, {1, 2}, {1, 3}, {1, 5} };
  for (int i = 0; i < (int)(sizeof(fractions)/sizeof(fractions[0])); i++) {
    if (from_image_start - fractions[i] * every_ms > 0) {
      return fractions[i] * every_ms;
    }
  }
  return 0L;
}

/// This function will constantly report to the instance's socket its current
/// state, untill it can get a hold of the g_mtx (global) mutex. Reporting is
/// performed in an every_ms millisecond interval.
/// Note that (in cotrast to AcquisitionReporter::report) this function should
/// report the whole time while (a diffrent thread) acquires a whole series
/// of images.
/// @note The andor2k API starts counting images in a series from index 1 (not
/// 0) e.g. for GetImages()
/// @note The current image index is set by the thread/function actually taking
/// the exposures. This function will not ever set this variable
/// (cur_img_in_series), it will only read it.
void AcquisitionSeriesReporter::report() noexcept {

  // time when function started
  // auto series_start_t = high_resolution_clock::now();

  // estimate the time for the whole series to end
  long total_millisec = estimated_series_time(exposure_millisec, num_images);

  // current image (note that the indexing of images starts from 1)
  int cur_img = 1;
  auto cur_img_start_t = series_start_t;

  // get a lock instance but do not try to lock yet; we should only be able
  // to get a hold of the lock after the series has ended (by that time, the
  // main thread should unlock the g_mtx)
  std::unique_lock<std::mutex> lk(g_mtx, std::defer_lock);

  long from_cimage_start = 0, from_acquisition_start = 0;
  int image_done = 0, series_done = 0;
  while (!lk.try_lock()) {
    // time now
    auto t_now = high_resolution_clock::now();

    // time since started this exposure
    from_cimage_start =
        duration_cast<std::chrono::milliseconds>(t_now - cur_img_start_t)
            .count();

    // time since start of exposure series
    from_acquisition_start =
        duration_cast<std::chrono::milliseconds>(t_now - series_start_t)
            .count();

    // what's the current image nr?
    if (cur_img != cur_img_in_series) { // previous image done! report that
// adjust start times; this exposure started sometime while we were
// sleeping...
#ifdef NEW_PART
      long adjust_ms = adjust_timing(from_cimage_start, every_millisec);
      auto from_cimage_start_p = from_cimage_start - adjust_ms;
      auto from_acquisition_start_p = from_acquisition_start - adjust_ms;
      cur_img_start_t =
          high_resolution_clock::now() - duration(milliseconds(adjust_ms));
#else
      // new start if current image
      cur_img_start_t = high_resolution_clock::now() -
                        duration(milliseconds(every_millisec / 2));
      auto from_cimage_start_p = from_cimage_start;
      auto from_acquisition_start_p = from_acquisition_start;
#endif

      // report that we finished previous image
      series_done = (from_acquisition_start_p * 100 / total_millisec);
      date_str(mbuf + len_const_prt);
      std::sprintf(
          mbuf + std::strlen(mbuf),
          ";status:Acquired image "
          "%d/%d;progperc:%d;sprogperc:%d;elapsedt:%.1f;selapsedt:%.1f;",
          cur_img, num_images, 100, series_done, from_cimage_start_p / 1e3,
          from_acquisition_start_p / 1e3);

      // send message to client via the socket
      socket->send(mbuf);
      cur_img = cur_img_in_series;
    }

    // percentage of current exposure finished
    image_done = (from_cimage_start * 100 /
                  estimated_time_per_image(exposure_millisec, cur_img));

    // percentage of series finished
    series_done = (from_acquisition_start * 100 / total_millisec);

    // prepare message to be sent (add datetime and indormation)
    date_str(mbuf + len_const_prt);
    std::sprintf(mbuf + std::strlen(mbuf),
                 ";status:Acquiring image "
                 "%d/%d;progperc:%d;sprogperc:%d;elapsedt:%.1f;selapsedt:%.1f;",
                 cur_img, num_images, image_done, series_done,
                 from_cimage_start / 1e3, from_acquisition_start / 1e3);
    // send message to client via the buffer
    socket->send(mbuf);

    // sleep a bit ...
    std::this_thread::sleep_for(std::chrono::milliseconds(every_millisec));
  }

  // we got a lock! that means that the exposure series is over
  lk.unlock();

  // prepare last message to be sent (add datetime and information)
  date_str(mbuf + len_const_prt);
  std::sprintf(mbuf + std::strlen(mbuf),
               ";status:Acquired %d/%d "
               "images;progperc:%d;sprogperc:%d;elapsedt:%.1f;selapsedt:%.1f",
               cur_img, num_images, 100, series_done, from_cimage_start / 1e3,
               from_acquisition_start / 1e3);

  // all done
  return;

} // report
