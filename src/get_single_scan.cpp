#include "acquisition_reporter.hpp"
#include "andor2k.hpp"
#include "andor_time_utils.hpp"
#include "atmcdLXd.h"
#include "fits_header.hpp"
#include <chrono>
#include <condition_variable>
#include <cppfits.hpp>
#include <cstdarg>
#include <cstdio>
#include <cstring>
#include <mutex>
#include <thread>

using andor2k::Socket;

extern std::mutex g_mtx;
extern std::mutex g_mtx_abort;
extern int abort_set;
extern int abort_socket_fd;
extern std::condition_variable cv;

auto rs_lambda = [](AcquisitionReporter reporter) { reporter.report(); };

/// @brief Get/Save a single scan acquisitionto FITS format
/// The function will perform the following:
/// * StartAcquisition
/// * GetAcquiredData
/// * Save to FITS file (using int32_t)
/// @param[in] params Currently not used in the function
/// @param[in] xpixels Number of x-axis pixels, aka width
/// @param[in] ypixels Number of y-axis pixels, aka height
/// @param[in] img_buffer An array of int32_t large enough to hold
///                    xpixels*ypixels elements
/// @note remember to set the (extern) variables:
/// extern int stop_reporting_thread;
/// extern int acquisition_thread_finished;
int get_single_scan(const AndorParameters *params, FitsHeaders *fheaders,
                    int xpixels, int ypixels, at_32 *img_buffer,
                    const Socket &socket) noexcept {

  char buf[32] = {'\0'};                  // buffer for datetime string
  char fits_filename[MAX_FITS_FILE_SIZE]; // FITS to save aqcuired data to
  char sockbuf[MAX_SOCKET_BUFFER_SIZE];   // buffer for socket reporting

  // first off, let's create an abort signal listener thread/socket. spawn off
  // the thread and wait till we are notified that we have the socket's fd
  abort_socket_fd = -100;
  std::thread abort_t(abort_listener, SOCKET_PORT + 1);
  std::this_thread::sleep_for(std::chrono::milliseconds(100));
  std::unique_lock<std::mutex> lk(g_mtx_abort);
  cv.wait(lk, [] { return abort_socket_fd; });
  lk.unlock();
  // ok, we are listening on port SOCKET_PORT+1 for abort, with the open socket
  // having an fd=abort_socket_fd

  // lets get the actual exposure time, so that we know exaclty
  float exposure, accumulate, kinetic;
  GetAcquisitionTimings(&exposure, &accumulate, &kinetic);

  // long millisec_per_image, total_millisec;
  // if (coarse_exposure_time(params, millisec_per_image, total_millisec)) {
  //  fprintf(stderr,
  //          "[ERROR][%s] Failed to compute coarse timings for acquisition! "
  //          "(traceback %s)\n",
  //          date_str(buf), __func__);
  //  millisec_per_image = static_cast<long>(params->exposure_ * 1e3);
  //  total_millisec = params->num_images_ * millisec_per_image;
  //}

  printf("[DEBUG][%s] Starting image acquisition ... with dimensions: %dx%d "
         "stored at %p\n",
         date_str(buf), xpixels, ypixels, (void *)img_buffer);

  // start acquisition ...
  auto acq_start_t = std::chrono::high_resolution_clock::now();
  if (unsigned error = StartAcquisition(); error != DRV_SUCCESS) {
    // failed to start acquisition .... report error and send it to client
    char acq_str[MAX_STATUS_STRING_SIZE];
    fprintf(stderr,
            "[ERROR][%s] Failed to start acquisition; error is: %s (traceback: "
            "%s)\n",
            date_str(buf), get_start_acquisition_status_string(error, acq_str),
            __func__);
    socket_sprintf(socket, sockbuf,
                   "done;error:1;info:start acquisition error (%s);time:%s;",
                   acq_str, buf);
    AbortAcquisition();
    // kill abort listening socket and join corresponding thread
    shutdown(abort_socket_fd, 2);
    abort_t.join();
    return 1;
  }

  // IMPORTANT!!
  // get the global lock -- only release it when we are done with the
  // acquisition
  g_mtx.lock();

  // spawn off a new thread to report status while we are waiting for the
  // acquisition to end
  std::thread report_t(
      rs_lambda,
      AcquisitionReporter(&socket, (long)(exposure * 1e3), acq_start_t));

  // wait for the acquisition ... (note that the already active abort-
  // listening socket, may receive an abort request while waiting (in which
  // case, abort_set should be positive)
  int status = WaitForAcquisition();
  if (status != DRV_SUCCESS) { // error while waiting for acquisition to end...
    fprintf(stderr,
            "[ERROR][%s] Something happened while waiting for a new "
            "acquisition! Aborting (traceback: %s)\n",
            date_str(buf), __func__);
    AbortAcquisition();

    // allow reporter to end and join with main, and
    // kill abort listening socket and join corresponding thread
    g_mtx.unlock();
    shutdown(abort_socket_fd, 2);
    report_t.join();
    abort_t.join();

    // report the error (maybe an abort requested by client)
    if (abort_set) {
      fprintf(stderr,
              "[ERROR][%s] Abort requested by client while waiting for a new "
              "acquisition! Aborting (traceback: %s)\n",
              date_str(buf), __func__);
      socket_sprintf(
          socket, sockbuf,
          "done;status:unfinished (abort called by user);error:%d;time%s;",
          status, date_str(buf));
    } else {
      socket_sprintf(socket, sockbuf,
                     "done;status:failed/error while waiting "
                     "acquisition;error:%d;time:%s;",
                     status, date_str(buf));
    }
    return 1;
  }

  // at this point, either we successefully waited or not, we should unlock
  // ... but we have not joined yet ....
  // by the way, shutdown the listening socket (on the abort thread)
  g_mtx.unlock();
  shutdown(abort_socket_fd, 2);

  // get the acquired data and set the timer for end of acquisition
  unsigned int error = GetAcquiredData(img_buffer, xpixels * ypixels);
  // auto acq_stop_t = std::chrono::high_resolution_clock::now();

  // enough time should have passed. join reporting and listening threads now
  report_t.join();
  abort_t.join();

  /* start of exposure time point is now, minus the correction
  auto dtnow = std::chrono::high_resolution_clock::now();
  dtnow -= std::chrono::milliseconds(start_time_corr);
  char tbuf[64];
  strfdt<DateTimeFormat::YMD>(dtnow, tbuf);
  fheaders->update("DATE", tbuf, "Date of exposure start");
  strfdt<DateTimeFormat::HMfS>(dtnow, tbuf);
  fheaders->update("DATE", tbuf, "UT time of exposure time"); */

  // check for errors while getting acquired data
  if (error != DRV_SUCCESS) {
    // report error
    char errbuf[MAX_STATUS_STRING_SIZE];
    fprintf(stderr,
            "[ERROR][%s] Failed to get acquired data! Aborting acquisition, "
            "error: %s "
            "(traceback: %s)\n",
            date_str(buf), get_get_acquired_data_status_string(error, errbuf),
            __func__);
    // abort acquisition
    AbortAcquisition();
    // report to client that an error occured
    socket_sprintf(socket, sockbuf,
                   "done;status:error (failed getting acquired data, "
                   "%s);error:%u;time:%s;",
                   errbuf, error, buf);
    return 2;
  }

  // report to client that data is acquired
  socket_sprintf(
      socket, sockbuf,
      "info:image data acquired;status:saving to FITS ...;image:1/1;time:%s;",
      date_str(buf));

  // save image to FITS format
  if (save_as_fits(params, fheaders, xpixels, ypixels, img_buffer, socket,
                   fits_filename, sockbuf))
    return 1;

  // auto ful_stop_at = std::chrono::high_resolution_clock::now();
  socket_sprintf(socket, sockbuf,
                 "done;error:0;info:FITS %s;status:image saving done;time:%s;",
                 fits_filename, date_str(buf));

  return 0;
}
