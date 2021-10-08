#include "andor2k.hpp"
#include "andor2kd.hpp"
#include "andor_time_utils.hpp"
#include "atmcdLXd.h"
#include "fits_header.hpp"
#include "get_exposure.hpp"
#include <algorithm>
#include <chrono>
#include <cppfits.hpp>
#include <cstdio>
#include <cstring>
#include <thread>

using andor2k::Socket;
using namespace std::chrono_literals;
using std_time_point = std::chrono::system_clock::time_point;

extern int sig_interrupt_set;
extern int sig_abort_set;
extern int abort_exposure_set;
extern int stop_reporting_thread;
extern int acquisition_thread_finished;

/*int get_single_scan(const AndorParameters *params, FitsHeaders *fheaders,
                    int xpixels, int ypixels, at_32 *img_buffer,
                    const Socket &socket) noexcept;*/
/*int get_rta_scan(const AndorParameters *params, FitsHeaders *fheaders,
                 int xpixels, int ypixels, at_32 *img_buffer,
                 const Socket &socket) noexcept;*/
int get_kinetic_scan(const AndorParameters *params, FitsHeaders *fheaders,
                     int xpixels, int ypixels, at_32 *img_buffer,
                     const Socket &socket) noexcept;

int find_start_time_cor(const FitsHeaders *fheaders,
                        long &correction_ns) noexcept {
  auto it = std::find_if(
      fheaders->mvec.begin(), fheaders->mvec.end(),
      [&](const FitsHeader &f) { return !std::strcmp(f.key, "TIMECORR"); });
  if (it != fheaders->mvec.cend()) {
    correction_ns = it->lval;
    return 0;
  }
  correction_ns = 0;
  return 1;
}

int exposure2tick_every(long iexp) noexcept {
  long min_tick = static_cast<long>(0.5e0 * 1e3);
  long max_tick = static_cast<long>(5 * 1e3);
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

class ThreadReporter {
public:
  ThreadReporter(const Socket *s, long exp_msec, long tot_ms, int img_nr,
                 int num_img, const std_time_point &s_start) noexcept
      : socket(s), image_nr(img_nr), num_images(num_img), exposure_ms(exp_msec),
        total_ms(tot_ms), series_start(s_start) {
    every_ms = exposure2tick_every(exp_msec);
    clear_buf();
    len_const_prt = std::sprintf(
        mbuf, "info:acquiring image ...;status:acquiring;image:%03d/%03d;time:",
        image_nr, num_images);
  };

  void report() noexcept {
    int it = 0;
    auto t_start = std::chrono::high_resolution_clock::now();
    while (!stop_reporting_thread && !acquisition_thread_finished) {
      auto t_now = std::chrono::high_resolution_clock::now();
      long from_thread_start =
          std::chrono::duration_cast<std::chrono::milliseconds>(t_now - t_start)
              .count();
      long from_acquisition_start =
          std::chrono::duration_cast<std::chrono::milliseconds>(t_now -
                                                                series_start)
              .count();
      int image_done = (from_thread_start * 100 / exposure_ms);
      int series_done = (from_acquisition_start * 100 / total_ms);

      date_str(mbuf + len_const_prt);
      std::sprintf(mbuf + std::strlen(mbuf),
                   ";progperc:%d;sprogperc:%d;elapsedt:%.2f;selapsedt:%.2f",
                   image_done, series_done, from_thread_start / 1e3,
                   from_acquisition_start / 1e3);
      socket->send(mbuf);

      if (stop_reporting_thread || acquisition_thread_finished)
        break;
      std::this_thread::sleep_for(std::chrono::milliseconds(every_ms));
      ++it;
    }
  }

private:
  long every_ms;        // sleeps for every_ms milliseconds, then reports
  const Socket *socket; // socket to send message to
  int image_nr,
      num_images; // current image number, total number of images is sequence
  char mbuf[MAX_SOCKET_BUFFER_SIZE]; // char buffer (for message)
  int len_const_prt;           // length of the constant part of the message
  long exposure_ms;            // exposure
  long total_ms;               //
  std_time_point series_start; // start of series

  void clear_buf() noexcept { std::memset(mbuf, 0, MAX_SOCKET_BUFFER_SIZE); }
};

void wait_for_acquisition(int &status) noexcept {
  acquisition_thread_finished = false;
  status = WaitForAcquisition();
  if (status != DRV_SUCCESS) {
    /*fprintf(stderr,
            "[ERROR][%s] Something happened while waiting for a new "
            "acquisition! Aborting (traceback: %s)\n",
            date_str(buf), __func__);*/
    AbortAcquisition();
    status = 1;
  }
  stop_reporting_thread = true;
  acquisition_thread_finished = true;
  status = 0;
  return;
}

auto report_lambda = [](ThreadReporter reporter) { reporter.report(); };

/// @brief Setup and get an acquisition (single or multiple scans)
/// The function will:
/// * get the acquisition(s) and store in FITS file(s)
/// Prior to calling this function, a call to setup_acquisition MUST have been
/// performed, so that the [xy]pixels should have been computed and storage
/// allocated for saving the exposures.
/// @param[in] params An AndorParameters instance holding information on the
///            acquisition we are going to setup.
/// @param[out] width The actual number of pixels in the X-dimension that the
///            acquired exposures are going to have. See setup_acquisition
/// @param[out] height The actual number of pixels in the Y-dimension that the
///            acquired exposures are going to have. See setup_acquisition
/// @param[out] img_mem A buffer where enough memory is allocated to store one
///            exposure based on input paramaeters. That means that the total
///            memory alocated is width * height * sizeof(at_32)
int get_acquisition(const AndorParameters *params, FitsHeaders *fheaders,
                    int xnumpixels, int ynumpixels, at_32 *img_buffer,
                    const Socket &socket) noexcept {

  char buf[32] = {'\0'}; // buffer for datetime string

#ifdef DEBUG
  printf("[DEBUG][%s] get_acquisition called, with dimensions %dx%d, to be "
         "stored at %p (traceback: %s)\n",
         date_str(buf), xnumpixels, ynumpixels, (void *)img_buffer, __func__);
#endif

  // depending on acquisition mode, acquire the exposure(s)
  int acq_status = 0;
  switch (params->acquisition_mode_) {
  case AcquisitionMode::SingleScan:
    acq_status = get_single_scan(params, fheaders, xnumpixels, ynumpixels,
                                 img_buffer, socket);
    break;
  case AcquisitionMode::RunTillAbort:
    acq_status = get_rta_scan(params, fheaders, xnumpixels, ynumpixels,
                              img_buffer, socket);
    break;
  case AcquisitionMode::KineticSeries:
    acq_status = get_kinetic_scan(params, fheaders, xnumpixels, ynumpixels,
                                  img_buffer, socket);
    break;
  default:
    fprintf(stderr,
            "[ERROR][%s] Invalid Acquisition Mode; don't know what to do! "
            "(traceback: %s)\n",
            date_str(buf), __func__);
    acq_status = 10;
  }

  // check for errors
  if (acq_status) {
    fprintf(stderr, "[ERROR][%s] Failed acquiring image(s)! (traceback: %s)\n",
            date_str(buf), __func__);
  }

  return acq_status;
}

/// @brief Get/Save a Kinetic acquisition to FITS format
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
int get_kinetic_scan(const AndorParameters *params, FitsHeaders *fheaders,
                     int xpixels, int ypixels, at_32 *img_buffer,
                     const Socket &socket) noexcept {

  char buf[32] = {'\0'};                  // buffer for datetime string
  char fits_filename[MAX_FITS_FILE_SIZE]; // FITS to save aqcuired data to
  char sbuf[MAX_SOCKET_BUFFER_SIZE];      // buffer for socket communication

  long millisec_per_image, total_millisec;
  if (coarse_exposure_time(params, millisec_per_image, total_millisec)) {
    fprintf(stderr,
            "[ERROR][%s] Failed to compute coarse timings for acquisition! "
            "(traceback %s)\n",
            date_str(buf), __func__);
    millisec_per_image = static_cast<long>(params->exposure_ * 1e3);
    total_millisec = params->num_images_ * millisec_per_image;
  }

  printf("---> KS: Computed image time: %ld and series time: %ld <--\n",
         millisec_per_image, total_millisec);

  // start acquisition(s)
  printf("[DEBUG][%s] Starting %d image acquisitions ...\n", date_str(buf),
         params->num_images_);
  auto series_start = std::chrono::system_clock::now();
  StartAcquisition();

  at_32 lAcquired = 0;
  while (lAcquired < params->num_images_) { // loop untill we have all images

    // start reporting thread for this image
    int cur_image = lAcquired + 1;
    stop_reporting_thread = false;
    std::thread rthread(report_lambda,
                        ThreadReporter(&socket, millisec_per_image,
                                       total_millisec, cur_image,
                                       params->num_images_, series_start));
    printf("--> new thread created, %d/%d ... <---\n", cur_image,
           params->num_images_);

    // do we have a signal to quit ?
    if (sig_abort_set || sig_interrupt_set) {
      stop_reporting_thread = true;
      rthread.join();
      return sig_abort_set ? ABORT_EXIT_STATUS : INTERRUPT_EXIT_STATUS;
    }

    // wait until acquisition finished
    if (WaitForAcquisition() != DRV_SUCCESS) {
      fprintf(stderr,
              "[ERROR][%s] Something happened while waiting for a new "
              "acquisition! Aborting (traceback: %s)\n",
              date_str(buf), __func__);
      AbortAcquisition();
      stop_reporting_thread = true;
      rthread.join();
      return 10;
    }

    // acquisition finished; stop reporting via the thread
    stop_reporting_thread = true;
    rthread.join();
    printf("--> thread joined <---\n");

    // total number of images acquired since the current acquisition started
    GetTotalNumberImagesAcquired(&lAcquired);

    // update the data array with the most recently acquired image
    if (unsigned int err = GetMostRecentImage(img_buffer, xpixels * ypixels);
        err != DRV_SUCCESS) {
      fprintf(stderr,
              "[ERROR][%s] Failed retrieving acquisition from cammera buffer! "
              "(traceback: %s)\n",
              date_str(buf), __func__);
      switch (err) {
      case DRV_ERROR_ACK:
        fprintf(stderr,
                "[ERROR][%s] Unable to communicate with card (traceback: %s)\n",
                date_str(buf), __func__);
        break;
      case DRV_P1INVALID:
        fprintf(stderr, "[ERROR][%s] Invalid pointer (traceback: %s)\n",
                date_str(buf), __func__);
        break;
      case DRV_P2INVALID:
        fprintf(stderr, "[ERROR][%s] Array size is incorrect (traceback: %s)\n",
                date_str(buf), __func__);
        break;
      case DRV_NO_NEW_DATA:
        fprintf(stderr,
                "[ERROR][%s] There is no new data yet (traceback: %s)\n",
                date_str(buf), __func__);
        break;
      }
      AbortAcquisition();
      socket_sprintf(socket, sbuf, "done;error:10;status:error;error:%u", err);
      return 10;
    }

    // save to FITS format
    if (get_next_fits_filename(params, fits_filename)) {
      fprintf(stderr,
              "[ERROR][%s] Failed getting FITS filename! No FITS image saved "
              "(traceback: %s)\n",
              date_str(buf), __func__);
      AbortAcquisition();
      return 1;
    }

    FitsImage<int32_t> fits(fits_filename, xpixels, ypixels);
    if (fits.write<at_32>(img_buffer)) {
      fprintf(stderr,
              "[ERROR][%s] Failed writting data to FITS file (traceback: "
              "%s)!\n",
              date_str(buf), __func__);
      AbortAcquisition();
      return 2;
    } else {
      printf("[DEBUG][%s] Image written in FITS file %s\n", date_str(buf),
             fits_filename);
    }

    if (fits.apply_headers(*fheaders, false) < 0) {
      fprintf(stderr,
              "[WRNNG][%s] Some headers not applied in FITS file! Should "
              "inspect file (traceback: %s)\n",
              date_str(buf), __func__);
    }
    fits.close();
  } // colected/saved all exposures!

  printf("[DEBUG][%s] Finished acquiring/saving %d images for sequence\n",
         date_str(buf), (int)lAcquired);
  socket_sprintf(socket, sbuf,
                 "done;error:0;info:images acquired and saved %ld", lAcquired);
  printf("--> sending [%s] <--\n", sbuf);

  AbortAcquisition();
  return 0;
}

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
/*int get_rta_scan(const AndorParameters *params, FitsHeaders *fheaders,
                 int xpixels, int ypixels, at_32 *img_buffer,
                 const Socket &socket) noexcept {

  char buf[32] = {'\0'};                  // buffer for datetime string
  char fits_filename[MAX_FITS_FILE_SIZE]; // FITS to save aqcuired data to
  char sbuf[MAX_SOCKET_BUFFER_SIZE];      // buffer for socket communication

  long millisec_per_image, total_millisec;
  if (coarse_exposure_time(params, millisec_per_image, total_millisec)) {
    fprintf(stderr,
            "[ERROR][%s] Failed to compute coarse timings for acquisition! "
            "(traceback %s)\n",
            date_str(buf), __func__);
    millisec_per_image = static_cast<long>(params->exposure_ * 1e3);
    total_millisec = params->num_images_ * millisec_per_image;
  }

  printf("---> RTA: Computed image time: %ld and series time: %ld <--\n",
         millisec_per_image, total_millisec);

  // start acquisition(s)
  printf("[DEBUG][%s] Starting %d image acquisitions ...\n", date_str(buf),
         params->num_images_);
  auto series_start = std::chrono::system_clock::now();
  StartAcquisition();

  at_32 lAcquired = 0;
  while (lAcquired < params->num_images_) { // loop untill we have all images

    // start reporting thread for this image
    int cur_image = lAcquired + 1;
    stop_reporting_thread = false;
    std::thread rthread(report_lambda,
                        ThreadReporter(&socket, millisec_per_image,
                                       total_millisec, cur_image,
                                       params->num_images_, series_start));
    printf("--> new thread created, %d/%d ... <---\n", cur_image,
           params->num_images_);

    // do we have a signal to quit ?
    if (sig_abort_set || sig_interrupt_set) {
      stop_reporting_thread = true;
      rthread.join();
      return sig_abort_set ? ABORT_EXIT_STATUS : INTERRUPT_EXIT_STATUS;
    }

    // wait until current image acquisition is finished
    if (WaitForAcquisition() != DRV_SUCCESS) {
      fprintf(stderr,
              "[ERROR][%s] Something happened while waiting for a new "
              "acquisition! Aborting (traceback: %s)\n",
              date_str(buf), __func__);
      AbortAcquisition();
      stop_reporting_thread = true;
      rthread.join();
      return 10;
    }

    // acquisition finished; stop reporting via the thread
    stop_reporting_thread = true;
    rthread.join();
    printf("--> thread joined <---\n");

    // total number of images acquired since the current acquisition started
    GetTotalNumberImagesAcquired(&lAcquired);

    // update the data array with the most recently acquired image
    if (unsigned int err = GetMostRecentImage(img_buffer, xpixels * ypixels);
        err != DRV_SUCCESS) {
      fprintf(stderr,
              "[ERROR][%s] Failed retrieving acquisition from cammera buffer! "
              "(traceback: %s)\n",
              date_str(buf), __func__);
      switch (err) {
      case DRV_ERROR_ACK:
        fprintf(stderr,
                "[ERROR][%s] Unable to communicate with card (traceback: %s)\n",
                date_str(buf), __func__);
        break;
      case DRV_P1INVALID:
        fprintf(stderr, "[ERROR][%s] Invalid pointer (traceback: %s)\n",
                date_str(buf), __func__);
        break;
      case DRV_P2INVALID:
        fprintf(stderr, "[ERROR][%s] Array size is incorrect (traceback: %s)\n",
                date_str(buf), __func__);
        break;
      case DRV_NO_NEW_DATA:
        fprintf(stderr,
                "[ERROR][%s] There is no new data yet (traceback: %s)\n",
                date_str(buf), __func__);
        break;
      }
      AbortAcquisition();
      socket_sprintf(socket, sbuf, "done;error:10;status:error;error:%u", err);
      return 10;
    }

    // get a FITS filename according to conventions
    if (get_next_fits_filename(params, fits_filename)) {
      fprintf(stderr,
              "[ERROR][%s] Failed getting FITS filename! No FITS image saved "
              "(traceback: %s)\n",
              date_str(buf), __func__);
      AbortAcquisition();
      socket_sprintf(socket, sbuf, "done;error:1;status:error;error:%u", 1);
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
      return 2;
    } else {
      printf("[DEBUG][%s] Image written in FITS file %s\n", date_str(buf),
             fits_filename);
    }

    // apply collected headers to the FITS file
    if (fits.apply_headers(*fheaders, false) < 0) {
      fprintf(stderr,
              "[WRNNG][%s] Some headers not applied in FITS file! Should "
              "inspect file (traceback: %s)\n",
              date_str(buf), __func__);
    }
    fits.close();
  } // colected/saved all exposures!

  printf("[DEBUG][%s] Finished acquiring/saving %d images for sequence\n",
         date_str(buf), (int)lAcquired);
  socket_sprintf(socket, sbuf,
                 "done;error:0;info:images acquired and saved %ld;status:idle",
                 lAcquired);
  printf("--> sending [%s] <--\n", sbuf);

  AbortAcquisition();
  return 0;
}*/

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
/*
int get_single_scan(const AndorParameters *params, FitsHeaders *fheaders,
                    int xpixels, int ypixels, at_32 *img_buffer,
                    const Socket &socket) noexcept {

  char buf[32] = {'\0'};                  // buffer for datetime string
  char fits_filename[MAX_FITS_FILE_SIZE]; // FITS to save aqcuired data to
  char sbuf[MAX_SOCKET_BUFFER_SIZE];

  // get start time correction in nanoseconds from headers (should have already
  // been computed)
  long start_time_corr;
  if (find_start_time_cor(fheaders, start_time_corr)) {
    fprintf(stderr,
            "[ERROR][%s] Failed to find TIMECORR header in the list! It should "
            "be there (traceback: %s)\n",
            date_str(buf), __func__);
    return 1;
  }

  long millisec_per_image, total_millisec;
  if (coarse_exposure_time(params, millisec_per_image, total_millisec)) {
    fprintf(stderr,
            "[ERROR][%s] Failed to compute coarse timings for acquisition! "
            "(traceback %s)\n",
            date_str(buf), __func__);
    millisec_per_image = static_cast<long>(params->exposure_ * 1e3);
    total_millisec = params->num_images_ * millisec_per_image;
  }

  // start acquisition and get date
  printf("[DEBUG][%s] Starting image acquisition ... with dimensions: %dx%d "
         "stored at %p\n",
         date_str(buf), xpixels, ypixels, (void *)img_buffer);

  auto exp_start_at = std::chrono::high_resolution_clock::now();
  stop_reporting_thread = false;
  acquisition_thread_finished = false;
  std::thread rthread(
      report_lambda, ThreadReporter(&socket, millisec_per_image, total_millisec,
                                    1, params->num_images_, exp_start_at));

  if (unsigned error = StartAcquisition(); error != DRV_SUCCESS) {
    char acq_str[MAX_STATUS_STRING_SIZE];
    fprintf(stderr, "[ERROR][%s] Failed to start acquisition; error is:\n",
            date_str(buf));
    fprintf(stderr, "[ERROR][%s] %s", date_str(buf),
            get_start_acquisition_status_string(error, acq_str));
    stop_reporting_thread = true;
    AbortAcquisition();
    acquisition_thread_finished = true;
    rthread.join();
    socket_sprintf(socket, sbuf,
                   "done;error:1;info:start acquisition error, image %d/%d", 1,
                   1);
    return 1;
  }

#ifdef DEBUG
//  char xbuf[64];
//  printf("[DEBUG][%s] TimingInfo --> StartAcquisition at -> %s\n",
//         date_str(buf), strfdt<DateTimeFormat::YMDHMfS>(at_start, xbuf));
#endif

//  // get status and loop until acquisition finished
//  int status;
//  GetStatus(&status);
//
//  while (status == DRV_ACQUIRING)
//    GetStatus(&status);
//  auto exp_stop_at = std::chrono::high_resolution_clock::now();
//#ifdef DEBUG
//  //at_start = std::chrono::high_resolution_clock::now();
//  //printf("[DEBUG][%s] TimingInfo --> After Acquisition stoped -> %s\n",
//  //       date_str(buf), strfdt<DateTimeFormat::YMDHMfS>(at_start, xbuf));
//#endif
//    stop_reporting = true;
//    rthread.join();
//
//  if (WaitForAcquisition() != DRV_SUCCESS) {
//    fprintf(stderr,
//            "[ERROR][%s] Something happened while waiting for a new "
//            "acquisition! Aborting (traceback: %s)\n",
//            date_str(buf), __func__);
//    AbortAcquisition();
//    stop_reporting = true;
//    rthread.join();
//    socket_sprintf(socket, sbuf, "done;error:1;status:error;error:%d", 10);
//    return 10;
//
  int wait_acquisition_status;
  abort_exposure_set = 0;
  std::thread athread(wait_for_acquisition, std::ref(wait_acquisition_status));

  while (!acquisition_thread_finished) {
    if (abort_exposure_set) {
      CancelWait();
      AbortAcquisition();
      printf("[DEBUG][%s] Abort exposure set! Aborting exposure ...\n",
             date_str(buf));
      stop_reporting_thread = true;
      acquisition_thread_finished = true;
      athread.join();
      rthread.join();
      socket_sprintf(socket, sbuf,
                     "done;status:error (abort acquisition requested);error:%d",
                     100);
      return 1;
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(500));
  }

  // waiting for acquisition ended ok
  athread.join();
  auto exp_stop_at = std::chrono::high_resolution_clock::now();
  stop_reporting_thread = true;
  rthread.join();
  if (wait_acquisition_status) {
    fprintf(stderr,
            "[ERROR][%s] Error while waiting for acquisition to end! "
            "(traceback: %s)\n",
            date_str(buf), __func__);
    socket_sprintf(socket, sbuf, "done;error:1;status:error;error:%d", 10);
    return 110;
  }

#ifdef USE_USHORT
  unsigned short *uimg_buffer = new unsigned short[xpixels * ypixels];
  printf(
      "[WRNNG][%s] Using the unsigned short version because of no binning!\n",
      date_str(buf));
  unsigned int error = GetAcquiredData16(uimg_buffer, xpixels * ypixels);
#else
  unsigned int error = GetAcquiredData(img_buffer, xpixels * ypixels);
  auto acq_stop_at = std::chrono::high_resolution_clock::now();
#endif

#ifdef DEBUG
//  at_start = std::chrono::high_resolution_clock::now();
//  printf("[DEBUG][%s] TimingInfo --> After Acquired Data -> %s\n",
//         date_str(buf), strfdt<DateTimeFormat::YMDHMfS>(at_start, xbuf));
#endif

  // start of exposure time point is now, minus the correction
  auto dtnow = std::chrono::high_resolution_clock::now();
  dtnow -= std::chrono::milliseconds(start_time_corr);
  char tbuf[64];
  strfdt<DateTimeFormat::YMD>(dtnow, tbuf);
  fheaders->update("DATE", tbuf, "Date of exposure start");
  strfdt<DateTimeFormat::HMfS>(dtnow, tbuf);
  fheaders->update("DATE", tbuf, "UT time of exposure time");

  // check for errors
  int retstat = 0;
  if (error != DRV_SUCCESS) {
    fprintf(stderr,
            "[ERROR][%s] Failed to get acquired data! Aborting acquisition "
            "(traceback: %s)\n",
            date_str(buf), __func__);
    retstat = 100;
    switch (error) {
    case DRV_ACQUIRING:
      fprintf(stderr,
              "[ERROR][%s] Error Message: Acquisition in progress (traceback: "
              "%s)\n",
              date_str(buf), __func__);
      retstat = 1;
      break;
    case DRV_ERROR_ACK:
      fprintf(stderr,
              "[ERROR][%s] Error Message: Unable to communicate with card "
              "(traceback: %s)\n",
              date_str(buf), __func__);
      retstat = 2;
      break;
    case DRV_P1INVALID:
      fprintf(stderr,
              "[ERROR][%s] Error Message: Invalid pointer (traceback: %s)\n",
              date_str(buf), __func__);
      retstat = 3;
      break;
    case DRV_P2INVALID:
      fprintf(stderr,
              "[ERROR][%s] Error Message: Array size is incorrect (traceback: "
              "%s)\n",
              date_str(buf), __func__);
      retstat = 4;
      break;
    case DRV_NO_NEW_DATA:
      fprintf(stderr,
              "[ERROR][%s] Error Message: No acquisition has taken place "
              "(traceback: %s)\n",
              date_str(buf), __func__);
      retstat = 5;
      break;
    default:
      fprintf(stderr,
              "[ERROR][%s] Error Message: Undocumented error (traceback: %s)\n",
              date_str(buf), __func__);
      retstat = 6;
    }
    // an error occured
    AbortAcquisition();
    socket_sprintf(socket, sbuf,
                   "done;status:error (failed getting acquired data);error:%d",
                   retstat);
    return retstat;
  }

  // formulate a valid FITS filename to save the data to
  if (get_next_fits_filename(params, fits_filename)) {
    fprintf(stderr,
            "[ERROR][%s] Failed getting FITS filename! No FITS image saved "
            "(traceback: %s)\n",
            date_str(buf), __func__);
    AbortAcquisition();
    socket_sprintf(socket, sbuf, "done;error:1;status:error;error:%d", 1);
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
    socket_sprintf(socket, sbuf, "done;error:1;status:error;error:%d", 1);
    return 15;
  } else {
    printf("[DEBUG][%s] Image written in FITS file %s\n", date_str(buf),
           fits_filename);
  }
  if (fits.apply_headers(*fheaders, false) < 0) {
    fprintf(stderr,
            "[WRNNG][%s] Some headers not applied in FITS file! Should inspect "
            "file (traceback: %s)\n",
            date_str(buf), __func__);
  }
  fits.close();

  auto ful_stop_at = std::chrono::high_resolution_clock::now();
  socket_sprintf(socket, sbuf,
                 "done;error:0;status:image saving done;info:image %d/%d", 0,
                 1);
  printf("---> sending via socket: %s <---\n", sbuf);

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

  return 0;
}*/
