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

/// @brief Get/Save a Run Till Abort acquisition to FITS format
/// The function will perform the following:
/// * StartAcquisition
/// * GetAcquiredData
/// * Save to FITS file (using int32_t)
/// (above steps are looped for number of images required)
/// * AbortAcquisition
/// @param[in] params Currently not used in the function
/// @param[in] xpixels Number of x-axis pixels, aka width
/// @param[in] ypixels Number of y-axis pixels, aka height
/// @param[in] img_buffer An array of int32_t large enough to hold
///                    xpixels*ypixels elements
/// @note before each (new) acquisition, we are checking the
/// sig_kill_acquisition (extern) variable; if set to true, we are going to
/// abort and return a negative integer.
int get_rta_scan(const AndorParameters *params, FitsHeaders *fheaders,
                 int xpixels, int ypixels, at_32 *img_buffer,
                 const Socket &socket) noexcept {

  char buf[32] = {'\0'};                  // buffer for datetime string
  char fits_filename[MAX_FITS_FILE_SIZE]; // FITS to save aqcuired data to
  char sockbuf[MAX_SOCKET_BUFFER_SIZE];      // buffer for socket communication
  
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

  printf("[DEBUG][%s] Starting RTA %d image acquisitions ... with dimensions: %dx%d "
         "stored at %p\n",
         date_str(buf), params->num_images_, xpixels, ypixels, (void *)img_buffer);
  
  // start time for the whole series
  auto series_start = std::chrono::system_clock::now();
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

  // loop untill we have all images
  for (int curimg=0; curimg<params->num_images_; curimg++) {

    //std::thread rthread(report_lambda,
    //                    ThreadReporter(&socket, millisec_per_image,
    //                                   total_millisec, cur_image,
    //                                   params->num_images_, series_start));
    //printf("--> new thread created, %d/%d ... <---\n", cur_image,
    //       params->num_images_);

#ifdef DEBUG
  printf("[DEBUG][%s] Performing acquisition for image %d/%d ...\n", date_str(buf),curimg+1, params->num_images_);
#endif

    // wait until current image acquisition is finished
    int status = WaitForAcquisition();
    if (status != DRV_SUCCESS) {
      fprintf(stderr,
              "[ERROR][%s] Something happened while waiting for a new "
              "acquisition! Aborting (traceback: %s)\n",
              date_str(buf), __func__);
      AbortAcquisition();
    
      // allow reporter to end and join with main, and
      // kill abort listening socket and join corresponding thread
      g_mtx.unlock();
      shutdown(abort_socket_fd, 2);
      // report_t.join();
      abort_t.join();
      
      // report the error (maybe an abort requested by client)
      if (abort_set) {
        fprintf(stderr,
                "[ERROR][%s] Abort requested by client while waiting for a new "
                "acquisition! Aborting (traceback: %s)\n",
                date_str(buf), __func__);
        socket_sprintf(socket, sockbuf,
                       "done;status:unfinished %d/%d (abort called by user);error:%d",
                       curimg+1, params->num_images_, status);
      } else {
        socket_sprintf(socket, sockbuf,
                       "done;status:failed/error %d/%d while waiting acquisition;error:%d",
                       curimg+1, params->num_images_, status);
      }
      return 1;
    }

#ifdef DEBUG
  printf("[DEBUG][%s] Exposure ended for image %d/%d ...\n", date_str(buf), curimg+1, params->num_images_);
#endif

    long validfirst = 0, validlast = 0;
    // update the data array with the most recently acquired image
    unsigned error = GetImages(curimg, curimg, img_buffer, xpixels*ypixels, &validfirst, &validlast);

    if (error != DRV_SUCCESS) {
      fprintf(stderr,
              "[ERROR][%s] Failed retrieving acquisition from cammera buffer! "
              "(traceback: %s)\n",
              date_str(buf), __func__);
      AbortAcquisition();
      // allow reporter to end and join with main, and
      // kill abort listening socket and join corresponding thread
      g_mtx.unlock();
      shutdown(abort_socket_fd, 2);
      // report_t.join();
      abort_t.join();
      socket_sprintf(socket, sockbuf,
        "done;status:failed/error %d/%d while waiting acquisition;error:%d",
                       curimg+1, params->num_images_, status);
      return 2;
    }

#ifdef DEBUG
  printf("[DEBUG][%s] Image acquired and saved to buffer for %d/%d\n", date_str(buf), curimg, params->num_images_);
#endif

    // get a FITS filename according to conventions
    if (get_next_fits_filename(params, fits_filename)) {
      fprintf(stderr,
              "[ERROR][%s] Failed getting FITS filename! No FITS image saved "
              "(traceback: %s)\n",
              date_str(buf), __func__);
      AbortAcquisition();
      // allow reporter to end and join with main, and
      // kill abort listening socket and join corresponding thread
      g_mtx.unlock();
      shutdown(abort_socket_fd, 2);
      // report_t.join();
      abort_t.join();
      socket_sprintf(socket, sockbuf, "done;status:error saving FITS file (%d/%d);error:%d", curimg+1, params->num_images_, 1);
      return 1;
    }

    // create a FITS file and save the image
    FitsImage<int32_t> fits(fits_filename, xpixels, ypixels);
    if (fits.write<at_32>(img_buffer)) {
      fprintf(stderr,
              "[ERROR][%s] Failed writting data to FITS file (traceback: "
              "%s)!\n",
              date_str(buf), __func__);
      AbortAcquisition();
      // allow reporter to end and join with main, and
      // kill abort listening socket and join corresponding thread
      g_mtx.unlock();
      shutdown(abort_socket_fd, 2);
      // report_t.join();
      abort_t.join();
      socket_sprintf(socket, sockbuf, "done;error:1;status:error while saving to FITS, image (%d/%d);error:%d", curimg+1, params->num_images_, 2);
      return 2;
    } else {
      printf("[DEBUG][%s] Image written in FITS file %s\n", date_str(buf),
             fits_filename);
      socket_sprintf(socket, sockbuf, "info:image %d/%d saved to FITS;status:FITS file created %s", curimg+1, params->num_images_, fits_filename);
    }

    // apply collected headers to the FITS file
    if (fits.apply_headers(*fheaders, false) < 0) {
      fprintf(stderr,
              "[WRNNG][%s] Some headers not applied in FITS file! Should "
              "inspect file (traceback: %s)\n",
              date_str(buf), __func__);
    }
    
    // close the newly created FITS file
    fits.close();

  } // colected/saved all exposures!

  // Series done! allow reporter to end and join with main, and kill abort 
  // listening socket and join corresponding thread
  g_mtx.unlock();
  shutdown(abort_socket_fd, 2);
  // report_t.join();
  abort_t.join();
  
  // auto ful_stop_at = std::chrono::high_resolution_clock::now();
  socket_sprintf(socket, sockbuf,
                 "done;error:0;info:exposure series ok;status:acquired and saved %d/%d images;", params->num_images_, params->num_images_);

  /*
  printf("[DEBUG][%s] Finished acquiring/saving %d images for sequence\n",
         date_str(buf), (int)lAcquired);
  socket_sprintf(socket, sbuf,
                 "done;error:0;info:images acquired and saved %ld;status:idle",
                 lAcquired);
  printf("--> sending [%s] <--\n", sbuf);
  */

  AbortAcquisition();
  return 0;
}
