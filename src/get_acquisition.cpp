#include "andor2k.hpp"
#include "andor_time_utils.hpp"
#include "atmcdLXd.h"
#include "cpp_socket.hpp"
#include "cppfits.hpp"
#include "fits_header.hpp"
#include <algorithm>
#include <chrono>
#include <cstdarg>
#include <cstdio>
#include <cstring>
#include <thread>

using andor2k::Socket;
using namespace std::chrono_literals;

extern int sig_interrupt_set;
extern int sig_abort_set;

int get_single_scan(const AndorParameters *params, FitsHeaders *fheaders,
                    int xpixels, int ypixels, at_32 *img_buffer,
                    const Socket &nsocket) noexcept;
int get_rta_scan(const AndorParameters *params, FitsHeaders *fheaders,
                 int xpixels, int ypixels, at_32 *img_buffer,
                 const Socket &socket) noexcept;
int get_kinetic_scan(const AndorParameters *params, FitsHeaders *fheaders,
                     int xpixels, int ypixels, at_32 *img_buffer,
                     const Socket &socket) noexcept;

int respond_to_client(const Socket &socket, char *buffer, const char *fmt,
                      ...) noexcept {
  va_list args;
  va_start(args, fmt);
  std::memset(buffer, 0, MAX_SOCKET_BUFFER_SIZE);
  char dtbuf[64] = ";time:";
  std::sprintf(buffer, fmt, args);
  std::strcat(buffer, date_str(dtbuf + 6));
  return socket.send(buffer);
}

long respond_every(float exposure_sec) noexcept {
  if (exposure_sec < 2.1) {
    return 0;
  } else if (exposure_sec < 4.1) {
    return 1000L;
  } else if (exposure_sec < 10.1) {
    return 1500L;
  } else if (exposure_sec < 60.1) {
    return 2500L;
  } else if (exposure_sec < 120.1) {
    return 3500L;
  } else {
    return 5000L;
  }
}

int percent_done(float exposure_sec, int iteration,
                 long sleep_time_ms) noexcept {
  float elapsed_sec = iteration * (sleep_time_ms / 1e3);
  return static_cast<int>(elapsed_sec * 100e0 / exposure_sec);
}

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

#ifdef CPPSTD_THREAD
bool stop_reporting;
void report_thread(long sleep_ms, const Socket *socket, float exposure_sec,
                   int image_nr, int num_images) noexcept {
  char sbuffer[MAX_SOCKET_BUFFER_SIZE];
  int it = 0;
  while (!stop_reporting) {
    respond_to_client(*socket, sbuffer,
                      "status:idle;info:acquiring ...;image:%d/%d;progperc:%d",
                      image_nr, num_images,
                      percent_done(exposure_sec, it, sleep_ms));
    std::this_thread::sleep_for(std::chrono::milliseconds(sleep_ms));
  }
}
#endif

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
  printf("[DEBUG][%s] tracing image memory; function %s, address: %p, is NULL "
         "%d\n",
         date_str(buf), __func__, (void *)&img_buffer,
         (int)(img_buffer == nullptr));
#endif
  if (img_buffer == nullptr) {
    fprintf(stderr,
            "[ERROR][%s] Memory location for images storage points to nowhere! "
            "(traceback: %s)\n",
            date_str(buf), __func__);
    return 1;
  }

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
  char sbuffer[MAX_SOCKET_BUFFER_SIZE];

  // start acquisition(s)
  printf("[DEBUG][%s] Starting (%d) image acquisitions ...\n", date_str(buf),
         params->num_images_);
  respond_to_client(
      socket, sbuffer,
      "status:idle;info:starting acquisition;image:1/%d;progperc:0",
      params->num_images_);
  std::this_thread::sleep_for(500ms);
  StartAcquisition();

#ifdef CPPSTD_THREAD
  long sleep_ms = respond_every(params->exposure_);
#endif
  at_32 lAcquired = 0;
  while (lAcquired < params->num_images_) { /* loop untill we have all images */

    // do we have a signal to quit ?
    if (sig_abort_set || sig_interrupt_set) {
      int exit_status =
          sig_abort_set ? ABORT_EXIT_STATUS : INTERRUPT_EXIT_STATUS;
      respond_to_client(socket, sbuffer,
                        "done;%d;status:abort;info:aborting "
                        "acquisition;image:%d/%d;progperc:0",
                        exit_status, lAcquired, params->num_images_);
      return exit_status;
    }

#ifdef CPPSTD_THREAD
    // create a new thread, that only has one job: report to client the exposure
    // progress status. To stop this thread, set the stop_reporting variable to
    // true
    stop_reporting = false;
    std::thread thread(report_thread, sleep_ms, &socket, params->exposure_,
                       lAcquired, params->num_images_);
#endif

    // wait until acquisition finished
    if (WaitForAcquisition() != DRV_SUCCESS) {
#ifdef CPPSTD_THREAD
      stop_reporting = true;
      thread.join();
      std::this_thread::sleep_for(500ms);
#endif
      fprintf(stderr,
              "[ERROR][%s] Something happened while waiting for a new "
              "acquisition! Aborting (traceback: %s)\n",
              date_str(buf), __func__);
      AbortAcquisition();
      respond_to_client(socket, sbuffer,
                        "done;10;status:abort;info:aborting "
                        "acquisition;image:%d/%d;progperc:0",
                        lAcquired, params->num_images_);
      return 10;
    }

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
      respond_to_client(socket, sbuffer,
                        "done;10;status:abort;info:data retrieval "
                        "failed;image:%d/%d;progperc:0",
                        lAcquired, params->num_images_);
      return 10;
    }

    std::this_thread::sleep_for(500ms);
    respond_to_client(socket, sbuffer,
                      "status:data retriveved;info:image acquisition "
                      "successeful;image:%d/%d;progperc:100",
                      lAcquired, params->num_images_);

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
  } // colected/saved all exposures

  printf("[DEBUG][%s] Finished acquiring/saving %d images for sequence\n",
         date_str(buf), (int)lAcquired);
  std::this_thread::sleep_for(500ms);
  respond_to_client(socket, sbuffer,
                    "done;0;status:finished;info:image sequence ended "
                    "successefully;image:%d/%d;progperc:100",
                    lAcquired, params->num_images_);

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
int get_rta_scan(const AndorParameters *params, FitsHeaders *fheaders,
                 int xpixels, int ypixels, at_32 *img_buffer,
                 const Socket &socket) noexcept {

  char buf[32] = {'\0'};                  // buffer for datetime string
  char fits_filename[MAX_FITS_FILE_SIZE]; // FITS to save aqcuired data to
  char sbuffer[MAX_SOCKET_BUFFER_SIZE];

  // start acquisition(s)
  printf("[DEBUG][%s] Starting (%d) image acquisitions ...\n", date_str(buf),
         params->num_images_);
  respond_to_client(
      socket, sbuffer,
      "status:idle;info:starting acquisition;image:1/%d;progperc:0",
      params->num_images_);
  std::this_thread::sleep_for(500ms);

#ifdef CPPSTD_THREAD
  long sleep_ms = respond_every(params->exposure_);
#endif
  StartAcquisition();

  at_32 lAcquired = 0;
  while (lAcquired < params->num_images_) { // loop untill we have all images

    // do we have a signal to quit ?
    if (sig_abort_set || sig_interrupt_set) {
      int exit_status =
          sig_abort_set ? ABORT_EXIT_STATUS : INTERRUPT_EXIT_STATUS;
      respond_to_client(socket, sbuffer,
                        "done;%d;status:abort;info:aborting "
                        "acquisition;image:%d/%d;progperc:0",
                        exit_status, lAcquired, params->num_images_);
      return exit_status;
    }

#ifdef CPPSTD_THREAD
    // create a new thread, that only has one job: report to client the exposure
    // progress status. To stop this thread, set the stop_reporting variable to
    // true
    stop_reporting = false;
    std::thread thread(report_thread, sleep_ms, &socket, params->exposure_,
                       lAcquired, params->num_images_);
#endif

    // wait until acquisition finished
    if (WaitForAcquisition() != DRV_SUCCESS) {
#ifdef CPPSTD_THREAD
      stop_reporting = true;
      thread.join();
      std::this_thread::sleep_for(500ms);
#endif
      fprintf(stderr,
              "[ERROR][%s] Something happened while waiting for a new "
              "acquisition! Aborting (traceback: %s)\n",
              date_str(buf), __func__);
      AbortAcquisition();
      respond_to_client(socket, sbuffer,
                        "done;10;status:abort;info:aborting "
                        "acquisition;image:%d/%d;progperc:0",
                        lAcquired, params->num_images_);
      return 10;
    }

#ifdef CPPSTD_THREAD
    stop_reporting = true;
    thread.join();
#endif

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
      respond_to_client(socket, sbuffer,
                        "done;10;status:abort;info:data retrieval "
                        "failed;image:%d/%d;progperc:0",
                        lAcquired, params->num_images_);
      return 10;
    }

    std::this_thread::sleep_for(500ms);
    respond_to_client(socket, sbuffer,
                      "status:data retriveved;info:image acquisition "
                      "successeful;image:%d/%d;progperc:100",
                      lAcquired, params->num_images_);

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

    respond_to_client(
        socket, sbuffer,
        "status:saved;info:image saved successefully;image:%d/%d;progperc:100",
        lAcquired, params->num_images_);
  } // colected/saved all exposures

  printf("[DEBUG][%s] Finished acquiring/saving %d images for sequence\n",
         date_str(buf), (int)lAcquired);
  std::this_thread::sleep_for(500ms);
  respond_to_client(socket, sbuffer,
                    "done;0;status:finished;info:image sequence ended "
                    "successefully;image:%d/%d;progperc:100",
                    lAcquired, params->num_images_);

  AbortAcquisition();
  return 0;
}

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
int get_single_scan(const AndorParameters *params, FitsHeaders *fheaders,
                    int xpixels, int ypixels, at_32 *img_buffer,
                    const Socket &socket) noexcept {

  char buf[32] = {'\0'};                  // buffer for datetime string
  char fits_filename[MAX_FITS_FILE_SIZE]; // FITS to save aqcuired data to
  char sbuffer[MAX_SOCKET_BUFFER_SIZE];

#ifdef DEBUG
  printf("[DEBUG][%s] tracing image memory; function %s, address: %p, is NULL "
         "%d\n",
         date_str(buf), __func__, (void *)&img_buffer,
         (int)(img_buffer == nullptr));
#endif

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

  // start acquisition and get date -- report to client
  printf("[DEBUG][%s] Starting image acquisition ...\n", date_str(buf));
  respond_to_client(
      socket, sbuffer,
      "status:idle;info:starting acquisition;image:1/1;progperc:0");
  std::this_thread::sleep_for(500ms);

#ifdef DEBUG
  auto at_start = std::chrono::high_resolution_clock::now();
#endif

  if (unsigned error = StartAcquisition(); error != DRV_SUCCESS) {
    char acq_str[MAX_STATUS_STRING_SIZE];
    fprintf(stderr, "[ERROR][%s] Failed to start acquisition; error is:\n",
            date_str(buf));
    fprintf(stderr, "[ERROR][%s] %s", date_str(buf),
            get_start_acquisition_status_string(error, acq_str));
    AbortAcquisition();
    respond_to_client(
        socket, sbuffer,
        "done;1;status:idle;info:acquisition error;image:1/1;progperc:0");
    return 1;
  }

#ifdef DEBUG
  char xbuf[64];
  printf("[DEBUG][%s] TimingInfo --> StartAcquisition at -> %s\n",
         date_str(buf), strfdt<DateTimeFormat::YMDHMfS>(at_start, xbuf));
#endif

  // get status and loop until acquisition finished
  int status;
  GetStatus(&status);

  long sleep_ms = respond_every(params->exposure_);
  int it = 0;
  while (status == DRV_ACQUIRING) {
    GetStatus(&status);
    respond_to_client(
        socket, sbuffer,
        "status:acquiring;info:performing acquisition;image:1/1;progperc:%d",
        percent_done(params->exposure_, it, sleep_ms));
    std::this_thread::sleep_for(std::chrono::milliseconds(sleep_ms));
    ++it;
  }

#ifdef DEBUG
  at_start = std::chrono::high_resolution_clock::now();
  printf("[DEBUG][%s] TimingInfo --> After Acquisition stoped -> %s\n",
         date_str(buf), strfdt<DateTimeFormat::YMDHMfS>(at_start, xbuf));
#endif

#ifdef USE_USHORT
  unsigned short *uimg_buffer = new unsigned short[xpixels * ypixels];
  printf(
      "[WRNNG][%s] Using the unsigned short version because of no binning!\n",
      date_str(buf));
  unsigned int error = GetAcquiredData16(uimg_buffer, xpixels * ypixels);
#else
  unsigned int error = GetAcquiredData(img_buffer, xpixels * ypixels);
#endif

#ifdef DEBUG
  at_start = std::chrono::high_resolution_clock::now();
  printf("[DEBUG][%s] TimingInfo --> After Acquired Data -> %s\n",
         date_str(buf), strfdt<DateTimeFormat::YMDHMfS>(at_start, xbuf));
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
    respond_to_client(
        socket, sbuffer,
        "done;1;status:acquiring;info:acquisition error;image:1/1;progperc:100",
        100);
#ifdef USE_USHORT
    delete[] uimg_buffer;
#endif
    return retstat;
  }

  // formulate a valid FITS filename to save the data to
  if (get_next_fits_filename(params, fits_filename)) {
    fprintf(stderr,
            "[ERROR][%s] Failed getting FITS filename! No FITS image saved "
            "(traceback: %s)\n",
            date_str(buf), __func__);
    AbortAcquisition();
#ifdef USE_USHORT
    delete[] uimg_buffer;
#endif
    return 1;
  }

  printf("[DEBUG][%s] Image acquired; saving to FITS file \"%s\" ...\n",
         date_str(buf), fits_filename);

// Create a FITS file and save the image at it
#ifdef USE_USHORT
  FitsImage<uint16_t> fits(fits_filename, xpixels, ypixels);
  if (fits.write<at_u16>(uimg_buffer)) {
#else
  FitsImage<int32_t> fits(fits_filename, xpixels, ypixels);
  if (fits.write<at_32>(img_buffer)) {
#endif
    fprintf(stderr,
            "[ERROR][%s] Failed writting data to FITS file (traceback: %s)!\n",
            date_str(buf), __func__);
#ifdef USE_USHORT
    delete[] uimg_buffer;
#endif
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

  respond_to_client(
      socket, sbuffer,
      "status:image saved;info:saved exposure to %s;image:1/1;progperc:%d",
      fits_filename, 100);
  std::this_thread::sleep_for(std::chrono::milliseconds(sleep_ms));
  respond_to_client(socket, sbuffer, "done;0");

#ifdef USE_USHORT
  delete[] uimg_buffer;
#endif

  // all done
  return 0;
}
