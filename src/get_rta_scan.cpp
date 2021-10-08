#include "acquisition_series_reporter.hpp"
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
extern int cur_img_in_series;
extern std::condition_variable cv;

auto rta_lambda = [](AcquisitionSeriesReporter reporter) { reporter.report(); };

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
  char sockbuf[MAX_SOCKET_BUFFER_SIZE];   // buffer for socket communication

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

  printf("[DEBUG][%s] Starting RTA %d image acquisitions ... with dimensions: "
         "%dx%d "
         "stored at %p\n",
         date_str(buf), params->num_images_, xpixels, ypixels,
         (void *)img_buffer);

  // lets get the actual exposure time, so that we know exaclty
  float exposure, accumulate, kinetic;
  GetAcquisitionTimings(&exposure, &accumulate, &kinetic);

  // start acquisition; start timing after the call to StartAcquisition, cause
  // this call will take some time ~250 millisec
  if (unsigned error = StartAcquisition(); error != DRV_SUCCESS) {
    // failed to start acquisition .... report error and send it to client
    char acq_str[MAX_STATUS_STRING_SIZE];
    fprintf(stderr,
            "[ERROR][%s] Failed to start acquisition; error descrition %s:\n",
            date_str(buf), get_start_acquisition_status_string(error, acq_str));

    socket_sprintf(
        socket, sockbuf,
        "done;error:%u;info:start acquisition error, image %d/%d;time:%s;",
        error, 1, 1, date_str(buf));

    AbortAcquisition();

    // kill abort listening socket and join corresponding thread
    shutdown(abort_socket_fd, 2);
    abort_t.join();
    return 1;
  }

  // start time for the whole series
  auto series_start = std::chrono::system_clock::now();

  // IMPORTANT!!
  // get the global lock -- only release it when we are done with the
  // acquisition
  g_mtx.lock();

  // spawn off a new thread to report status while we are waiting for the
  // acquisition to end
  std::thread report_t(
      rta_lambda, AcquisitionSeriesReporter(
                      &socket, (long)(exposure * 1000), params->num_images_,
                      std::chrono::high_resolution_clock::now()));

  // loop untill we have all images
  for (int curimg = 0; curimg < params->num_images_; curimg++) {
    cur_img_in_series = curimg + 1;

#ifdef DEBUG
    printf("[DEBUG][%s] Performing acquisition for image %d/%d ...\n",
           date_str(buf), cur_img_in_series, params->num_images_);
    auto wfa_ci = std::chrono::system_clock::now();
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
      report_t.join();
      abort_t.join();

      // report the error (maybe an abort requested by client)
      if (abort_set) {
        fprintf(stderr,
                "[ERROR][%s] Abort requested by client while waiting for a new "
                "acquisition! Aborting (traceback: %s)\n",
                date_str(buf), __func__);
        socket_sprintf(socket, sockbuf,
                       "done;status:unfinished %d/%d (abort called by "
                       "user);error:%d;time:%s;",
                       cur_img_in_series, params->num_images_, status,
                       date_str(buf));
      } else {
        socket_sprintf(socket, sockbuf,
                       "done;status:failed/error %d/%d while waiting "
                       "acquisition;error:%d;time:%s;",
                       cur_img_in_series, params->num_images_, status,
                       date_str(buf));
      }
      return 1;
    } // WaitForAcquisition()

#ifdef DEBUG
    printf(">> WaitForAcquisition took %ld millisec (image %d/%d)\n",
           std::chrono::duration_cast<std::chrono::milliseconds>(
               std::chrono::system_clock::now() - wfa_ci)
               .count(),
           cur_img_in_series, params->num_images_);
#endif

#ifdef DEBUG
    printf("[DEBUG][%s] Exposure ended for image %d/%d ...\n", date_str(buf),
           cur_img_in_series, params->num_images_);
    auto gi_ci = std::chrono::system_clock::now();
#endif

    // wait untill a new image is available
    int vfirst = 0, vlast = 0;
    unsigned error;
#ifdef DEBUG
    auto tt = std::chrono::system_clock::now();
#endif
    while (GetNumberNewImages(&vfirst, &vlast) == DRV_NO_NEW_DATA) {
      printf(">> No new data yet; waitin for new available image ...\n");
      std::this_thread::sleep_for(std::chrono::milliseconds(1));
#ifdef DEBUG
      if (std::chrono::duration_cast<std::chrono::seconds>(
              std::chrono::system_clock::now() - tt)
              .count() > 10) {
        printf(">> Exiting wated for 10 secs and still no new image!\n");
        AbortAcquisition();
        // allow reporter to end and join with main, and
        // kill abort listening socket and join corresponding thread
        g_mtx.unlock();
        shutdown(abort_socket_fd, 2);
        report_t.join();
        abort_t.join();
        socket_sprintf(socket, sockbuf,
                       "done;status:failed/error %d/%d while waiting "
                       "acquisition;error:%d;time:%s;",
                       cur_img_in_series, params->num_images_, status,
                       date_str(buf));
        return 1;
      }
#endif
    }

#ifdef DEBUG
    printf(">> GetNumberNewImages(&vfirst, &vlast) returned vfirst=%d and "
           "vlast=%d\n",
           vfirst, vlast);
#endif

    // get current image from circular buffer; copy to img_buffer
    error = GetImages(cur_img_in_series, cur_img_in_series, img_buffer,
                      xpixels * ypixels, &vfirst, &vlast);

#ifdef DEBUG
    printf(">> GetImages for image %d returned sizes: validfirst:%d, "
           "validlast:%d\n",
           cur_img_in_series, vfirst, vlast);
#endif

    // did we get the data successefully ?
    if (error != DRV_SUCCESS) {
      char errorbuf[MAX_STATUS_STRING_SIZE];
      fprintf(stderr,
              "[ERROR][%s] Failed retrieving acquisition from cammera buffer! "
              "Error: %s"
              "(traceback: %s)\n",
              date_str(buf), get_get_images_string(error, errorbuf), __func__);
      AbortAcquisition();

      // allow reporter to end and join with main, and
      // kill abort listening socket and join corresponding thread
      g_mtx.unlock();
      shutdown(abort_socket_fd, 2);
      report_t.join();
      abort_t.join();

      socket_sprintf(socket, sockbuf,
                     "done;status:failed/error image %d/%d while retrieving "
                     "data (%s);error:%u;time:%s;",
                     cur_img_in_series, params->num_images_, errorbuf, error,
                     date_str(buf));
      return 1;
    }

#ifdef DEBUG
    printf(">> GetImage took %ld millisec (image %d/%d)\n",
           std::chrono::duration_cast<std::chrono::milliseconds>(
               std::chrono::system_clock::now() - gi_ci)
               .count(),
           cur_img_in_series, params->num_images_);
    auto saf_ci = std::chrono::system_clock::now();
#endif

#ifdef DEBUG
    printf("[DEBUG][%s] Image acquired and saved to buffer for %d/%d\n",
           date_str(buf), cur_img_in_series, params->num_images_);
#endif

    // save image to FITS format
    if (save_as_fits(params, fheaders, xpixels, ypixels, img_buffer, socket,
                     fits_filename, sockbuf)) {
      AbortAcquisition();
      // allow reporter to end and join with main, and
      // kill abort listening socket and join corresponding thread
      g_mtx.unlock();
      shutdown(abort_socket_fd, 2);
      report_t.join();
      abort_t.join();
      return 1;
    }

    /* get a FITS filename according to conventions
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
      report_t.join();
      abort_t.join();
      socket_sprintf(socket, sockbuf, "done;status:error saving FITS file
    (%d/%d);error:%d;time:%s;", cur_img_in_series, params->num_images_, 1,
    date_str(buf)); return 1;
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
      report_t.join();
      abort_t.join();
      socket_sprintf(socket, sockbuf, "done;error:1;status:error while saving to
    FITS, image (%d/%d);error:%d;time:%s;", cur_img_in_series,
    params->num_images_, 2, date_str(buf)); return 2; } else {
      printf("[DEBUG][%s] Image written in FITS file %s\n", date_str(buf),
             fits_filename);
      //socket_sprintf(socket, sockbuf, "info:image %d/%d saved to
    FITS;status:FITS file created %s", curimg+1, params->num_images_,
    fits_filename);
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
    */

#ifdef DEBUG
    printf(">> SaveToFits took %ld millisec (image %d/%d)\n",
           std::chrono::duration_cast<std::chrono::milliseconds>(
               std::chrono::system_clock::now() - saf_ci)
               .count(),
           cur_img_in_series, params->num_images_);
#endif

  } // colected/saved all exposures!

#ifdef DEBUG
  printf(">> Series took %ld millisec\n",
         std::chrono::duration_cast<std::chrono::milliseconds>(
             std::chrono::system_clock::now() - series_start)
             .count());
#endif

  // Series done! allow reporter to end and join with main, and kill abort
  // listening socket and join corresponding thread
  g_mtx.unlock();
  shutdown(abort_socket_fd, 2);
  report_t.join();
  abort_t.join();

  // auto ful_stop_at = std::chrono::high_resolution_clock::now();
  socket_sprintf(socket, sockbuf,
                 "done;error:0;info:exposure series ok;status:acquired and "
                 "saved %d/%d images;time:%s;",
                 cur_img_in_series, params->num_images_, date_str(buf));

  AbortAcquisition();
  return 0;
}
