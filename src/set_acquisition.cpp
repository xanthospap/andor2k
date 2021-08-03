#include "andor2k.hpp"
#include "atmcdLXd.h"
#include <cstdio>
#include <cstring>

/// @brief Setup an acquisition (single or multiple scans).
/// The function will:
/// * setup the Read Mode
/// * setup the Acquisition Mode
/// * set vertical and horizontal Shift Speeds
/// * initialize the Shutter
/// * compute image dimensions (aka pixels in width and height)
/// * allocate memory for (temporarily) storing image data
/// On sucess, a call to get_acquisition should follow to actually perform
/// the acquisition.
/// @param[in] params An AndorParameters instance holding information on the
///            acquisition we are going to setup.
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
int setup_acquisition(const AndorParameters *params, int &width, int &height, float& vsspeed, float& hsspeed,
                      at_32 *img_mem) noexcept {

  char buf[32] = {'\0'}; /* buffer for datetime string */

  /* check and set read-out mode */
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

  /* set acquisition mode (this will also set the exposure time) */
  if (setup_acquisition_mode(params)) {
    fprintf(stderr,
            "[ERROR][%s] Failed to set acquisition mode! (traceback: %s)\n",
            date_str(buf), __func__);
    return 10;
  }

  /* set vertical and horizontal shift speeds */
  // float vspeed, hspeed;
  if (set_fastest_recomended_vh_speeds(vsspeed, hsspeed)) {
    fprintf(stderr,
            "[ERROR][%s] Failed to set shift speed(s)! (traceback: %s)\n",
            date_str(buf), __func__);
    return 10;
  }

  /* initialize shutter */
  unsigned int error =
      SetShutter(1, ShutterMode2int(params->shutter_mode_),
                 params->shutter_closing_time_, params->shutter_opening_time_);
  if (error != DRV_SUCCESS) {
    fprintf(stderr,
            "[ERROR][%s] Failed to initialize shutter! (traceback: %s)\n",
            date_str(buf), __func__);
    return 10;
  }

  /* get detector pixels */
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

  /* compute actual number of pixels (width and height) for the exposure(s) */
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

  /* allocate memory to (temporarily) hold the image data */
  long image_pixels = xnumpixels * ynumpixels;
  img_mem = new at_32[image_pixels];

  /* gcc wants smthng to be done with img_mem else it complains about an
   * unused variable; do something!
   */
#ifdef __GNUC__
#ifndef __clang__
  std::memset(img_mem, 0, image_pixels * sizeof(at_32));
#endif
#endif

  width = xnumpixels;
  height = ynumpixels;
  return 0;
}
