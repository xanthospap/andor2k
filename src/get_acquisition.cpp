#include "andor2k.hpp"
#include "atmcdLXd.h"
#include <chrono>
#include <cppfits.hpp>
#include <cstdio>
#include <cstring>
#include <thread>
#include "fits_header.hpp"

using namespace std::chrono_literals;

int get_single_scan(const AndorParameters *params, int xpixels, int ypixels,
                    at_32 *img_buffer) noexcept;
int get_rta_scan(const AndorParameters *params, int xpixels, int ypixels,
                 at_32 *img_buffer) noexcept;
int get_kinetic_scan(const AndorParameters *params, int xpixels, int ypixels,
                     at_32 *img_buffer) noexcept;
int get_rta_scan2(const AndorParameters *params, int xpixels, int ypixels,
                  at_32 *img_buffer) noexcept;

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
int get_acquisition(const AndorParameters *params, int xnumpixels,
                    int ynumpixels, at_32 *img_buffer) noexcept {

  char buf[32] = {'\0'}; /* buffer for datetime string */

  /* depending on acquisition mode, acquire the exposure(s) */
  int acq_status = 0;
  switch (params->acquisition_mode_) {
  case AcquisitionMode::SingleScan:
    acq_status = get_single_scan(params, xnumpixels, ynumpixels, img_buffer);
    break;
  case AcquisitionMode::RunTillAbort:
    acq_status = get_rta_scan(params, xnumpixels, ynumpixels, img_buffer);
    break;
  case AcquisitionMode::KineticSeries:
    acq_status = get_kinetic_scan(params, xnumpixels, ynumpixels, img_buffer);
    break;
  default:
    fprintf(stderr,
            "[ERROR][%s] Invalid Acquisition Mode; don't know what to do! "
            "(traceback: %s)\n",
            date_str(buf), __func__);
    acq_status = 10;
  }

  /* check for errors */
  if (acq_status) {
    fprintf(stderr, "[ERROR][%s] Failed acquiring image(s)! (traceback: %s)\n",
            date_str(buf), __func__);
  }

  /* free allocated memory
  delete[] img_buffer;
  */

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
int get_kinetic_scan(const AndorParameters *params, int xpixels, int ypixels,
                     at_32 *img_buffer) noexcept {

  char buf[32] = {'\0'};                  /* buffer for datetime string */
  char fits_filename[MAX_FITS_FILE_SIZE]; /* FITS to save aqcuired data to */

  /* start acquisition(s) */
  printf("[DEBUG][%s] Starting (%d) image acquisitions ...\n", date_str(buf),
         params->num_images_);
  StartAcquisition();

  at_32 lAcquired = 0;
  while (lAcquired < params->num_images_) { /* loop untill we have all images */

    /* do we have a signal to quit ? */
    if (sig_kill_acquisition) {
      sig_kill_acquisition = 0;
      return -1;
    }

    /* wait until acquisition finished */
    if (WaitForAcquisition() != DRV_SUCCESS) {
      fprintf(stderr,
              "[ERROR][%s] Something happened while waiting for a new "
              "acquisition! Aborting (traceback: %s)\n",
              date_str(buf), __func__);
      AbortAcquisition();
      return 10;
    }

    /* total number of images acquired since the current acquisition started */
    GetTotalNumberImagesAcquired(&lAcquired);

    /* update the data array with the most recently acquired image */
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
      return 10;
    }

    /* save to FITS format */
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
    fits.update_key<int>("NXAXIS1", &xpixels, "width");
    fits.update_key<int>("NXAXIS2", &ypixels, "height");
    fits.close();
  } /* colected/saved all exposures! */

  printf("[DEBUG][%s] Finished acquiring/saving %d images for sequence\n",
         date_str(buf), (int)lAcquired);

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
int get_rta_scan(const AndorParameters *params, int xpixels, int ypixels,
                 at_32 *img_buffer) noexcept {

  char buf[32] = {'\0'};                  /* buffer for datetime string */
  char fits_filename[MAX_FITS_FILE_SIZE]; /* FITS to save aqcuired data to */

  /* start acquisition(s) */
  printf("[DEBUG][%s] Starting (%d) image acquisitions ...\n", date_str(buf),
         params->num_images_);
  StartAcquisition();

  at_32 lAcquired = 0;
  while (lAcquired < params->num_images_) { /* loop untill we have all images */

    /* do we have a signal to quit ? */
    if (sig_kill_acquisition) {
      sig_kill_acquisition = 0;
      return -1;
    }

    /* wait until acquisition finished */
    if (WaitForAcquisition() != DRV_SUCCESS) {
      fprintf(stderr,
              "[ERROR][%s] Something happened while waiting for a new "
              "acquisition! Aborting (traceback: %s)\n",
              date_str(buf), __func__);
      AbortAcquisition();
      return 10;
    }

    /* total number of images acquired since the current acquisition started */
    GetTotalNumberImagesAcquired(&lAcquired);

    /* update the data array with the most recently acquired image */
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
      return 10;
    }

    /* save to FITS format */
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
    fits.update_key<int>("NXAXIS1", &xpixels, "width");
    fits.update_key<int>("NXAXIS2", &ypixels, "height");
    fits.close();
  } /* colected/saved all exposures! */

  printf("[DEBUG][%s] Finished acquiring/saving %d images for sequence\n",
         date_str(buf), (int)lAcquired);

  AbortAcquisition();
  return 0;
}

/// @brief Get/Save a Run Till Abort acquisition to FITS format
/// The function will perform the following:
/// * StartAcquisition
/// * GetAcquiredData
/// * Save to FITS file (using int32_t)
/// @param[in] params Currently not used in the function
/// @param[in] xpixels Number of x-axis pixels, aka width
/// @param[in] ypixels Number of y-axis pixels, aka height
/// @param[in] img_buffer An array of int32_t large enough to hold
///                    xpixels*ypixels elements
int get_rta_scan2(const AndorParameters *params, int xpixels, int ypixels,
                  at_32 *img_buffer) noexcept {

  char buf[32] = {'\0'};                  /* buffer for datetime string */
  char fits_filename[MAX_FITS_FILE_SIZE]; /* FITS to save aqcuired data to */

  /* start acquisition(s) */
  printf("[DEBUG][%s] Starting (%d) image acquisitions ...\n", date_str(buf),
         params->num_images_);
  StartAcquisition();

  unsigned int error;
  int images_remaining = params->num_images_;
  int buffer_images_remaining = 0;
  int buffer_images_retrieved = 0;
  int status;
  at_32 series = 0, first, last;

  GetTotalNumberImagesAcquired(&series);

  GetStatus(&status);
  auto acq_start_t = std::chrono::high_resolution_clock::now();
  while ((status == DRV_ACQUIRING && images_remaining > 0) ||
         buffer_images_remaining > 0) {
    auto nowt1 = std::chrono::high_resolution_clock::now();
    auto elapsed_time_sec1 =
        std::chrono::duration_cast<std::chrono::seconds>(nowt1 - acq_start_t);
    printf("[DEBUG][%s] Ellapsed time acquiring %10ld seconds; current series "
           "%d buf. images retrieved %d buf images remaining %d\n",
           date_str(buf), elapsed_time_sec1.count(), series,
           buffer_images_retrieved, buffer_images_remaining);
    if (images_remaining == 0)
      error = AbortAcquisition();

    GetTotalNumberImagesAcquired(&series);

    if (GetNumberNewImages(&first, &last) == DRV_SUCCESS) {
      buffer_images_remaining = last - first;
      images_remaining = params->num_images_ - series;

      error = GetOldestImage(img_buffer, xpixels * ypixels);

      if (error == DRV_P2INVALID || error == DRV_P1INVALID) {
        fprintf(stderr,
                "[ERROR][%s] Acquisition error, nr #%d (traceback: %s)\n",
                date_str(buf), error, __func__);
        fprintf(stderr, "[ERROR][%s] Aborting acquisition.\n", date_str(buf));
        AbortAcquisition();
      }

      if (error == DRV_SUCCESS) {
        ++buffer_images_retrieved;
        get_next_fits_filename(params, fits_filename);
        FitsImage<int32_t> fits(fits_filename, xpixels, ypixels);
        if (fits.write<at_32>(img_buffer)) {
          fprintf(stderr,
                  "[ERROR][%s] Failed writting data to FITS file (traceback: "
                  "%s)!\n",
                  date_str(buf), __func__);
          AbortAcquisition();
          // return 15;
        } else {
          printf("[DEBUG][%s] Image written in FITS file %s\n", date_str(buf),
                 fits_filename);
        }
        fits.update_key<int>("NXAXIS1", &xpixels, "width");
        fits.update_key<int>("NXAXIS2", &ypixels, "height");
        fits.close();
      }
    }

    // abort after full number of images taken
    if (series >= params->num_images_) {
      printf("[DEBUG][%s] Succesefully acquired all images (aka %d)\n",
             date_str(buf), series);
      AbortAcquisition();
    }

    GetStatus(&status);
    auto nowt = std::chrono::high_resolution_clock::now();
    auto elapsed_time_sec =
        std::chrono::duration_cast<std::chrono::seconds>(nowt - acq_start_t);
    /*if (elapsed_time_sec > 3 * 60 * 1000) {
      fprintf(stderr, "[ERROR][%s] Aborting acquisition cause it took too much
    time! (traceback: %s)\n", date_str(buf), __func__); AbortAcquisition();
      GetStatus(&status);
    }*/
    GetStatus(&status);
    printf("[DEBUG][%s] Ellapsed time acquiring %10ld seconds; current series "
           "%d buf. images retrieved %d buf images remaining %d\n",
           date_str(buf), elapsed_time_sec.count(), series,
           buffer_images_retrieved, buffer_images_remaining);
    std::this_thread::sleep_for(1000ms);
  }

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
int get_single_scan(const AndorParameters *params, int xpixels, int ypixels,
                    at_32 *img_buffer) noexcept {

  char buf[32] = {'\0'};                  /* buffer for datetime string */
  char fits_filename[MAX_FITS_FILE_SIZE]; /* FITS to save aqcuired data to */


  /* start acquisition */
  printf("[DEBUG][%s] Starting image acquisition ...\n", date_str(buf));
  StartAcquisition();

  /* get status and loop until acquisition finished */
  int status;
  GetStatus(&status);

  while (status == DRV_ACQUIRING)
    GetStatus(&status);

  /* get acquired data (from camera buffer) */
  unsigned int error = GetAcquiredData(img_buffer, xpixels * ypixels);

  /* check for errors */
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
    /* an error occured */
    AbortAcquisition();
    return retstat;
  }

  /* formulate a valid FITS filename to save the data to */
  if (get_next_fits_filename(params, fits_filename)) {
    fprintf(stderr,
            "[ERROR][%s] Failed getting FITS filename! No FITS image saved "
            "(traceback: %s)\n",
            date_str(buf), __func__);
    AbortAcquisition();
    return 1;
  }

  printf("[DEBUG][%s] Image acquired; saving to FITS file \"%s\" ...\n",
         date_str(buf), fits_filename);

  /*
  if (SaveAsFITS(fits_filename, 3) != DRV_SUCCESS)
    fprintf(stderr, "\n[ERROR][%s] Failed to save image to fits format!
  (traceback: %s)\n", date_str(buf), __func__);
  */

  /* Create a FITS file and save the image at it */
  FitsImage<int32_t> fits(fits_filename, xpixels, ypixels);
  if (fits.write<at_32>(img_buffer)) {
    fprintf(stderr,
            "[ERROR][%s] Failed writting data to FITS file (traceback: %s)!\n",
            date_str(buf), __func__);
    return 15;
  } else {
    printf("[DEBUG][%s] Image written in FITS file %s\n", date_str(buf),
           fits_filename);
  }
  fits.update_key<int>("NXAXIS1", &xpixels, "width");
  fits.update_key<int>("NXAXIS2", &ypixels, "height");
  fits.close();

  /* all done */
  return 0;
}


int collect_fits_headers(const AndorParameters *params, FitsHeaders& headers) noexcept {
  headers.clear();
  int error = 0;
  headers.force_update("INSTRUME", "ANDOR2K", "Name of instrument");
  headers.force_update("OBSTYPE", params->type_, "Image type");
  headers.force_update<int>("HBIN", params->image_hbin_, "Horizontal binning");
  headers.force_update<int>("VBIN", params->image_vbin_, "Vertical binning");
  return error;
}