#include "acquisition_series_reporter.hpp"
#include "cpp_socket.hpp"
#include "andor2k.hpp"
#include "andor2kd.hpp"
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
    estimated_time_per_image(exposure_millisec, 1)*(num_images-1) + 
    4*num_images;
}

AcquisitionSeriesReporter::AcquisitionSeriesReporter(const Socket *s, long exp_msec, int n_images,
                 const std_time_point &s_start) noexcept :
    socket(s),
    exposure_millisec(exp_msec),
    series_start(s_start),
    num_images(n_images),
    every_millisec(200)
  {
    // write constant part of message so that we don't have to write it every
    // time
    clear_buf();
    len_const_prt = std::sprintf(
        mbuf, "info:Acquiring image series...;time:");
  }

  // constantly report untill we can get a lock of the mutex
  void AcquisitionSeriesReporter::report() noexcept {

    // time when function started
    auto series_start_t = high_resolution_clock::now();

    // estimate the time for the whole series to end
    long total_millisec = estimated_series_time(exposure_millisec, num_images);

    // current image (note that the indexing of images starts from 1)
    int cur_img = 1;
    auto cur_img_start_t = series_start_t;

    // get a lock instance but do not try to lock yet; we should only be able
    // to get a hold of the lock after the series has ended (by that time, the
    // main thread should unlock the g_mtx)
    std::unique_lock<std::mutex> lk(g_mtx, std::defer_lock);

    long from_thread_start=0, from_acquisition_start=0;
    int image_done=0, series_done=0;
    while (!lk.try_lock()) {
      // time now
      auto t_now = high_resolution_clock::now();

      // time since started this exposure
      from_thread_start =
          duration_cast<std::chrono::milliseconds>(t_now - cur_img_start_t)
              .count();
      
      // time since start of exposure series
      from_acquisition_start =
          duration_cast<std::chrono::milliseconds>(t_now - series_start_t)
              .count();
      
      // what's the current image nr?
      if (cur_img != cur_img_in_series) {// previous image done! report that
        // new start if current image
        cur_img_start_t = high_resolution_clock::now() + 
          duration(milliseconds(every_millisec/2));
        
        // report that we finished previous image
        series_done = (from_acquisition_start * 100 / total_millisec);
        date_str(mbuf + len_const_prt);
        std::sprintf(mbuf + std::strlen(mbuf),
                     ";status:Acquired image %d/%d;progperc:%d;sprogperc:%d;elapsedt:%.1f;selapsedt:%.1f;",
                     cur_img, num_images, 100, series_done, from_thread_start / 1e3,
                     from_acquisition_start / 1e3);
        
        // send message to client via the buffer
        socket->send(mbuf);
        cur_img = cur_img_in_series;
      }

      // percentage of exposure finished
      image_done = (from_thread_start * 100 / estimated_time_per_image(exposure_millisec, cur_img));
      
      // percentage of series finished
      series_done = (from_acquisition_start * 100 / total_millisec);
      
      // prepare message to be sent (add datetime and indormation)
      date_str(mbuf + len_const_prt);
      std::sprintf(mbuf + std::strlen(mbuf),
                   ";status:Acquiring image %d/%d;progperc:%d;sprogperc:%d;elapsedt:%.1f;selapsedt:%.1f;",
                   cur_img,num_images,image_done, series_done, from_thread_start / 1e3,
                   from_acquisition_start / 1e3);
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
                 ";status:Acquired %d/%d images;progperc:%d;sprogperc:%d;elapsedt:%.1f;selapsedt:%.1f",
                 cur_img, num_images, 100, series_done, from_thread_start / 1e3,
                 from_acquisition_start / 1e3);
    
    // all done
    return;

}//report
