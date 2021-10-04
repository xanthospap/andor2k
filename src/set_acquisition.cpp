#include "andor2k.hpp"
#include "andor_time_utils.hpp"
#include "aristarchos.hpp"
#include "atmcdLXd.h"
#include "fits_header.hpp"
#include <cstdio>
#include <cstring>

/// @brief Setup an acquisition (single or multiple scans).
/// The function will:
/// * setup the Read Mode
/// * setup the Acquisition Mode
/// * set vertical and horizontal Shift Speeds
/// * initialize the Shutter
/// * compute image dimensions (aka pixels in width and height)
/// * if needed, get Aristarchos headers; add headers to the ones passed in
/// * allocate memory for (temporarily) storing image data
/// On sucess, a call to get_acquisition should follow to actually perform
/// the acquisition.
/// @param[in] params An AndorParameters instance holding information on the
///            acquisition we are going to setup.
/// @param[in] fheaders A FitsHeaders instance; where needed, the function will
///            add/update headers
/// @param[out] width The actual number of pixels in the X-dimension that the
///            acquired exposures are going to have.This number is ccomputed
///            based on the params instance
/// @param[out] height The actual number of pixels in the Y-dimension that the
///            acquired exposures are going to have.This number is ccomputed
///            based on the params instance
/// @param[out] vsspeed The vertical shift speed set for the exposure; units
///            are microseconds per pixel shift
/// @param[out] hsspeed The horizontal shift speed set for the exposure; units
///            are microseconds per pixel shift
/// @param[out] img_mem A buffer where enough memory is allocated to store one
///            exposure based on input paramaeters. That means that the total
///            memory alocated is width * height * sizeof(at_32)
/// @warning
/// - Note that you are now incharge of the memory allocated; the
///   memory should be freed/deleted after usage to avoid memory leaks
/// - The function will set times for the exposure(s) (e.g. kinnetic time,
///   exposure time, etc), but these might not be the actual ones used by the
///   ANDOR2K system! Use the function GetAcquisitionTimings to get the actual
///   times to be used
int setup_acquisition(const AndorParameters *params, FitsHeaders *fheaders,
                      int &width, int &height, float &vsspeed, float &hsspeed,
                      at_32 *&img_mem) noexcept {

  char buf[32] = {'\0'}; // buffer for datetime string

  // check and set read-out mode
  if (params->read_out_mode_ != ReadOutMode::Image) {
    fprintf(stderr,
            "[ERROR][%s] Can only acquire images in Image ReadOut Mode! "
            "(traceback: %s)\n",
            date_str(buf), __func__);
    return 1;
  }
  if (setup_read_out_mode(params)) {
    fprintf(stderr, "[ERROR][%s] Failed to set read mode! (traceback: %s)\n",
            date_str(buf), __func__);
    return 10;
  }

  // set acquisition mode (this will also set the exposure time)
  if (setup_acquisition_mode(params)) {
    fprintf(stderr,
            "[ERROR][%s] Failed to set acquisition mode! (traceback: %s)\n",
            date_str(buf), __func__);
    return 10;
  }

  // set vertical and horizontal shift speeds
  if (set_fastest_recomended_vh_speeds(vsspeed, hsspeed)) {
    fprintf(stderr,
            "[ERROR][%s] Failed to set shift speed(s)! (traceback: %s)\n",
            date_str(buf), __func__);
    return 10;
  }

  // initialize shutter
  unsigned int error =
      SetShutter(1, ShutterMode2int(params->shutter_mode_),
                 params->shutter_closing_time_, params->shutter_opening_time_);
  if (error != DRV_SUCCESS) {
    fprintf(stderr,
            "[ERROR][%s] Failed to initialize shutter! (traceback: %s)\n",
            date_str(buf), __func__);
    return 10;
  }

  // get detector pixels
  int xpixels, ypixels;
  if (GetDetector(&xpixels, &ypixels) != DRV_SUCCESS) {
    fprintf(stderr,
            "[ERROR][%s] Failed getting detector size! (traceback: %s)\n",
            date_str(buf), __func__);
    return 1;
  }
  if (xpixels != MAX_PIXELS_IN_DIM || ypixels != MAX_PIXELS_IN_DIM) {
    fprintf(stderr,
            "[ERROR][%s] Detector returned erronuous image size (%6dx%6d)! "
            "(traceback: %s)\n",
            date_str(buf), xpixels, ypixels, __func__);
    return 1;
  }

  // compute actual number of pixels (width and height) for the exposure(s)
  int xnumpixels = params->image_hend_ - params->image_hstart_ + 1;
  int ynumpixels = params->image_vend_ - params->image_vstart_ + 1;
  xnumpixels /= params->image_hbin_;
  ynumpixels /= params->image_vbin_;
  if ((xnumpixels < 1 || xnumpixels > MAX_PIXELS_IN_DIM) ||
      (ynumpixels < 1 || ynumpixels > MAX_PIXELS_IN_DIM)) {
    fprintf(
        stderr,
        "[ERROR][%s] Computed image size outside valid rage! (traceback: %s)\n",
        date_str(buf), __func__);
    return 1;
  }
  printf("[DEBUG][%s] Computed pixels = %5dx%5d\n", date_str(buf), xnumpixels,
         ynumpixels);
  printf("[DEBUG][%s] Detector pixels = %5dx%5d\n", date_str(buf), xpixels,
         ypixels);

  // try to get/decode Aristarchos headers if requested 
  // TODO that could be done in another thread to save us some time!
  if (params->ar_hdr_tries_ > 0) {
    std::vector<FitsHeader> ar_headers;
    ar_headers.reserve(150);
    if (get_aristarchos_headers(params->ar_hdr_tries_, ar_headers)) {
      fprintf(stderr,
              "[ERROR][%s] Failed to fetch/decode Aristarchos headers "
              "(traceback: %s)\n",
              date_str(buf), __func__);
      return 2;
    }
    if (fheaders->merge(ar_headers, true) < 0) {
      fprintf(stderr,
              "[ERROR][%s] Failed merging Aristarchos headers to the previous "
              "set! (traceback: %s)\n",
              date_str(buf), __func__);
      return 2;
    }
  }

  // add a few things to the headers
  float actual_exposure, actual_accumulate, actual_kinetic;
  if (GetAcquisitionTimings(&actual_exposure, &actual_accumulate,
                            &actual_kinetic) != DRV_SUCCESS) {
    fprintf(stderr,
            "[ERROR][%s] Failed to retrieve actual ANDOR2k-tuned acquisition "
            "timings! (traceback: %s)\n",
            date_str(buf), __func__);
    return 3;
  } else {
    printf("[DEBUG][%s] Actual acquistion timings tuned by the ANDOR2K system "
           "to be:\n",
           date_str(buf));
    printf("[DEBUG][%s] Exposure Time        : %.2f sec.\n", buf,
           actual_exposure);
    printf("[DEBUG][%s] Accumulate Cycle Time: %.2f sec.\n", buf,
           actual_accumulate);
    printf("[DEBUG][%s] Kinetic Cycle Time   : %.2f sec.\n", buf,
           actual_kinetic);
  }

  int herror;
  herror = fheaders->update<float>(
      "HSSPEED", hsspeed, "Horizontal Shift Speed (microsec / pixel shift)");
  if (herror < 0)
    fprintf(stderr, "[WRNNG][%s] Failed to update header for HSSPEED\n",
            date_str(buf));

  herror = fheaders->update<float>(
      "VSSPEED", vsspeed, "Vertical Shift Speed (microsec / pixel shift)");
  if (herror < 0)
    fprintf(stderr, "[WRNNG][%s] Failed to update header for VSSPEED\n",
            date_str(buf));

  herror = fheaders->update<float>("EXPOSED", actual_exposure,
                                   "Requested exposure time (sec)");
  if (herror < 0)
    fprintf(stderr, "[WRNNG][%s] Failed to update header for EXPOSED\n",
            date_str(buf));

  herror = fheaders->update<float>("EXPTIME", actual_exposure,
                                   "Requested exposure time (sec)");
  if (herror < 0)
    fprintf(stderr, "[WRNNG][%s] Failed to update header for EXPTIME\n",
            date_str(buf));

  herror =
      fheaders->update<int>("VBIN", params->image_vbin_, "Vertical binning");
  if (herror < 0)
    fprintf(stderr, "[WRNNG][%s] Failed to update header for VBIN\n",
            date_str(buf));

  herror =
      fheaders->update<int>("HBIN", params->image_hbin_, "Horizontal Binning");
  if (herror < 0)
    fprintf(stderr, "[WRNNG][%s] Failed to update header for HBIN\n",
            date_str(buf));

  herror = fheaders->update("INSTRUME", "ANDOR2048x2048_BV",
                            "Instrument used to acquire data");
  if (herror < 0)
    fprintf(stderr, "[WRNNG][%s] Failed to update header for INSTRUME\n",
            date_str(buf));

  herror =
      fheaders->update("OBJECT", params->object_name_, "Object identifier");
  if (herror < 0)
    fprintf(stderr, "[WRNNG][%s] Failed to update header for INSTRUME\n",
            date_str(buf));

  herror = fheaders->update("FILTER", params->filter_name_, "Filter used");
  if (herror < 0)
    fprintf(stderr, "[WRNNG][%s] Failed to update header for FILTER\n",
            date_str(buf));

  // herror = fheaders->update("ORIGIN", "NOA -- Mt. Chelmos", "Observatory");
  // if (herror < 0)
  //  fprintf(stderr, "[WRNNG][%s] Failed to update header for ORIGIN\n",
  //          date_str(buf));

  // herror = fheaders->update("TELESCOP", "Aristarchos",
  //                 "Telescope used to acquire data");
  // if (herror < 0)
  //  fprintf(stderr, "[WRNNG][%s] Failed to update header for TELESCOP\n",
  //          date_str(buf));

  // herror = fheaders->update<float>("HEIGHT", 2326.0, "height above sea level
  // [m]"); if (herror < 0)
  //  fprintf(stderr, "[WRNNG][%s] Failed to update header for HEIGHT\n",
  //          date_str(buf));

  // herror = fheaders->update("LATITUDE", "+37:59:35.30",
  //                 "Telescope latitude dd:mm:ss");
  // if (herror < 0)
  //  fprintf(stderr, "[WRNNG][%s] Failed to update header for LATITUDE\n",
  //          date_str(buf));

  // herror = fheaders->update("LONGITUD", "022:12:32.50",
  //                 "Telescope longitude dd:mm:ss");
  // if (herror < 0)
  //  fprintf(stderr, "[WRNNG][%s] Failed to update header for LONGITUDE\n",
  //          date_str(buf));

  // compute the start time correction for the headers
  auto start_time_cor = start_time_correction(actual_exposure, vsspeed, hsspeed,
                                              ynumpixels, xnumpixels);
  herror = fheaders->update<long>(
      "TIMECORR", start_time_cor.count(),
      "Timming correction already applied (nanosec)");
  if (herror < 0)
    fprintf(stderr, "[WRNNG][%s] Failed to update header for TIMECORR\n",
            date_str(buf));

  // get the camera's temperature for reporting in header
  float tempf;
  if (GetTemperatureF(&tempf) != DRV_TEMP_STABILIZED) {
    fprintf(stderr,
            "[ERROR][%s] Tried to get temperature for headers, but did not get "
            "a stabilized one! (traceback %s)\n",
            date_str(buf), __func__);
    // return 10;
  } else {
    herror =
        fheaders->update<float>("CCDTEMP", tempf,
                                "CCD temp at start of exposure degC"); // 10
    if (herror < 0)
      fprintf(stderr, "[WRNNG][%s] Failed to update header for CCDTEMP\n",
              date_str(buf));
  }

  if (herror != 10) {
    fprintf(stderr,
            "[ERROR][%s] Failed to add one or more headers to the list! "
            "(traceback: %s)\n",
            date_str(buf), __func__);
    // return 3;
  }

  // allocate memory to (temporarily) hold the image data
  long image_pixels = xnumpixels * ynumpixels;
  img_mem = nullptr;
  img_mem = new at_32[image_pixels];
  if (img_mem == nullptr) {
    fprintf(stderr,
            "[ERROR][%s] Failed to allocate memory for the image (%ld)! "
            "(traceback: %s)\n",
            date_str(buf), image_pixels, __func__);
    return 1;
  }
  printf("[DEBUG][%s] Allocated memory for image; size is %d*%d=%ld (adress: "
         "%p)\n",
         date_str(buf), xnumpixels, ynumpixels, image_pixels, (void *)img_mem);

  // gcc wants smthng to be done with img_mem else it complains about an
  // unused variable; do something!
#ifdef __GNUC__
#ifndef __clang__
  std::memset(img_mem, 0, image_pixels * sizeof(at_32));
#endif
#endif

  width = xnumpixels;
  height = ynumpixels;
  return 0;
}
