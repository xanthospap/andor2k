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
int set_fastest_recomended_vh_speeds(float &vspeed, int hsspeed_index,
                                     float &hsspeed_mhz) noexcept {
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

  // set horizontal speed shift
  error = SetHSSpeed(0, hsspeed_index);
  if (error == DRV_SUCCESS) {
    // get the speed (horizontal) in MHz for the specified index
    GetHSSpeed(0, 0, hsspeed_index, &hsspeed_mhz);
    printf(
        "[DEBUG][%s] Set Horizontal Shift Speed to index %d (i.e. %.3fMHz)\n",
        date_str(buf), hsspeed_index, hsspeed_mhz);
  } else {
    fprintf(stderr,
            "[ERROR][%s] Failed to set HSSpeed to index %d (traceback: %s)\n",
            date_str(buf), hsspeed_index, __func__);
  }

  return (error == DRV_SUCCESS ? 0 : 1);
}
