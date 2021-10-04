#include "andor2k.hpp"
#include "andor_time_utils.hpp"
#include "atmcdLXd.h"
#include "fits_header.hpp"
#include "acquisition_reporter.hpp"
#include "abort_listener.hpp"
#include <chrono>
#include <cppfits.hpp>
#include <cstdarg>
#include <cstdio>
#include <cstring>
#include <mutex>
#include <thread>
#include <condition_variable>

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
  std::thread abort_t(abort_listener, SOCKET_PORT+1);
  std::this_thread::sleep_for(std::chrono::milliseconds(100));
  std::unique_lock<std::mutex> lk(g_mtx_abort);
  cv.wait(lk, []{return abort_socket_fd;});
  lk.unlock();
  // ok, we are listening on port SOCKET_PORT+1 for abort, with the open socket 
  // having an fd=abort_socket_fd

  long millisec_per_image, total_millisec;
  if (coarse_exposure_time(params, millisec_per_image, total_millisec)) {
    fprintf(stderr,
            "[ERROR][%s] Failed to compute coarse timings for acquisition! "
            "(traceback %s)\n",
            date_str(buf), __func__);
    millisec_per_image = static_cast<long>(params->exposure_ * 1e3);
    total_millisec = params->num_images_ * millisec_per_image;
  }

  printf("[DEBUG][%s] Starting image acquisition ... with dimensions: %dx%d "
         "stored at %p\n",
         date_str(buf), xpixels, ypixels, (void *)img_buffer);

  // start acquisition ...
  auto acq_start_t = std::chrono::high_resolution_clock::now();
  if (unsigned error = StartAcquisition(); error != DRV_SUCCESS) {
    // failed to start acquisition .... report error and send it to client
    char acq_str[MAX_STATUS_STRING_SIZE];
    fprintf(stderr, "[ERROR][%s] Failed to start acquisition; error is:\n",
            date_str(buf));
    fprintf(stderr, "[ERROR][%s] %s", date_str(buf),
            get_start_acquisition_status_string(error, acq_str));
    socket_sprintf(socket, sockbuf,
                   "done;error:1;info:start acquisition error, image %d/%d", 1,
                   1);
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
    rs_lambda, AcquisitionReporter(&socket, millisec_per_image, total_millisec,
                              1, params->num_images_, acq_start_t));
  
  // wait for the acquisition ...
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
    fprintf(stderr,
            "[ERROR][%s] Failed to get acquired data! Aborting acquisition "
            "(traceback: %s)\n",
            date_str(buf), __func__);
    char errbuf[MAX_STATUS_STRING_SIZE];
    fprintf(stderr,
      "[ERROR][%s] Details: %s (traceback: %s)\n", buf, get_get_acquired_data_status_string(error, errbuf), __func__);
    // abort acquisition
    AbortAcquisition();
    // report to client that an error occured
    socket_sprintf(socket, sockbuf,
                   "done;status:error (failed getting acquired data);error:%u",
                   error);
    return 2;
  }
  
  // report to client that data is acquired
  socket_sprintf(socket, sockbuf, "info:image data acquired;status:saving to FITS ...;image:1/1;");

  // formulate a valid FITS filename to save the data to
  if (get_next_fits_filename(params, fits_filename)) {
    fprintf(stderr,
            "[ERROR][%s] Failed getting FITS filename! No FITS image saved "
            "(traceback: %s)\n",
            date_str(buf), __func__);
    AbortAcquisition();
    socket_sprintf(socket, sockbuf, "done;error:1;status:error;error:%d", 1);
    return 1;
  }

  printf("[DEBUG][%s] Image acquired; saving to FITS file \"%s\" ...\n",
         date_str(buf), fits_filename);

  // Create a FITS file and save the image at it
  FitsImage<int32_t> fits(fits_filename, xpixels, ypixels);
  if (fits.write<at_32>(img_buffer)) {
    fprintf(stderr,
            "[ERROR][%s] Failed writting data to FITS file (traceback: %s)!\n",
            date_str(buf), __func__);
    socket_sprintf(socket, sockbuf, "done;error:1;status:error while saving to FITS;error:%d", 15);
    return 15;
  } else {
    printf("[DEBUG][%s] Image written in FITS file %s\n", date_str(buf),
           fits_filename);
    socket_sprintf(socket, sockbuf, "info:image saved to FITS;status:FITS file created %s", fits_filename);
  }

  // apply headers to FITS file and close
  if (fits.apply_headers(*fheaders, false) < 0) {
    fprintf(stderr,
            "[WRNNG][%s] Some headers not applied in FITS file! Should inspect "
            "file (traceback: %s)\n",
            date_str(buf), __func__);
  }

  // close the (newly-created) FITS file
  fits.close();

  // auto ful_stop_at = std::chrono::high_resolution_clock::now();
  socket_sprintf(socket, sockbuf,
                 "done;error:0;info:FITS %s;status:image saving done;info:image %d/%d", fits_filename, 0,
                 1);
  /*
  printf("--> Timing Details <--\n");
  printf("\tExposure duration         %.3f millisec\n",
         std::chrono::duration<double, std::milli>(exp_stop_at - exp_start_at)
             .count());
  printf("\tAcquisition duration      %.3f millisec\n",
         std::chrono::duration<double, std::milli>(acq_stop_at - exp_stop_at)
             .count());
  printf("\tExposure plus acquisition %.3f millisec\n",
         std::chrono::duration<double, std::milli>(acq_stop_at - exp_start_at)
             .count());
  printf("\tFits manipulation         %.3f millisec\n",
         std::chrono::duration<double, std::milli>(ful_stop_at - acq_stop_at)
             .count());
  printf("\tFull acquisition          %.3f millisec\n",
         std::chrono::duration<double, std::milli>(ful_stop_at - exp_start_at)
             .count());
  */

  return 0;
}
