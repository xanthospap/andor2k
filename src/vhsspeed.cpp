#include "andor2k.hpp"
#include "atmcdLXd.h"
#include <cstdio>
#include <cstring>

/// @brief Set Vertical and Horizontal Shift Speeds
/// This function will set the Vertical Shift Speed to the fastest recommended
/// by the ANDOR2K system. It will also set the Horizontal Shift Speed to the
/// fastest speed available.
/// @param[out] vspeed Vertical Shift Speed set by the function in case of
///             sucess; units are microseconds per pixel shift
/// @param[out] hspeed Horizontal Shift Speed set by the function in case of
///             sucess; units are microseconds per pixel shift
/// @return Returns 0 if speeds are set correctly; in case of error, an integer
///         other than zero is returned
/// @note Note that while the ANDOR2K SDK has a function to get the recommended
/// vertical shift speed (aka GetFastestRecommendedVSSpeed), no such function
/// is available for the horizontal speed.
/// @todo does this need the SetFrameTransferMode to be set to 1?
int set_fastest_recomended_vh_speeds(float &vspeed, float &hspeed) noexcept {
  char buf[32];

  // set vertical shift speed to fastest recommended
  int index;
  unsigned int error = GetFastestRecommendedVSSpeed(&index, &vspeed);
  if (error != DRV_SUCCESS) {
    fprintf(stderr,
            "[ERROR][%s] Failed to get fastest recommended VSSpeed (traceback: "
            "%s)\n",
            date_str(buf), __func__);
    return 1;
  } else {
    error = SetVSSpeed(index);
    if (error != DRV_SUCCESS) {
      fprintf(stderr,
              "[ERROR][%s] Failed to set fastest recommended VSSpeed "
              "(traceback: %s)\n",
              date_str(buf), __func__);
      return 1;
    }
    printf("[DEBUG][%s] Set Vertical Speed Shift to fastest recommended; that "
           "is %8.2f microseconds per pixel shift\n",
           date_str(buf), vspeed);
  }

  // get horizontal speed shifts
  int num_hspeeds;
  error = GetNumberHSSpeeds(0, 0, &num_hspeeds);
  // note that the shift speeds are always returned fastest first
  if (error == DRV_SUCCESS) {
    GetHSSpeed(0, 0, 0, &hspeed); // get first speed, aka the fastest
    error = SetHSSpeed(0, 0);     // set the first speed, aka the fastest
  }
  if (error == DRV_SUCCESS) {
    printf("[DEBUG][%s] Set Horizontal Shift Speed to fastest, that is %8.2ff "
           "microseconds per pixel shift\n",
           date_str(buf), hspeed);
  } else {
    fprintf(stderr,
            "[ERROR][%s] Failed to set fastest HSSpeed (traceback: %s)\n",
            date_str(buf), __func__);
  }

  return (error == DRV_SUCCESS ? 0 : 1);
}
