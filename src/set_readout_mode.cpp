#include "andor2k.hpp"
#include "atmcdLXd.h"
#include <cstdio>
#include <limits>

/// @bief Set ReadOutMode for Andor2k camera
/// @see USER’S GUIDE TO SDK, Software Version 2.102
///
/// Possible Read Out Modes are:
/// * Full Vertical Binning
///   SetReadMode(0)
/// * Single Track
///   SetReadMode(3)
///   SetSingleTrack(center_row, track_height)
/// * Multi Track
///   not applicable; will produce an error
/// * Random Track
///   not applicable; will produce an error
/// * Image
///   SetReadMode(4)
///   SetImage(...)
///
/// @param[in] params An instance of type AndorParameters; the function will set
///            all needed parameters based on values stored in this instance
///            (including ReadOutMode, exposure time, etc).
///
/// @return An integer deonting the status of the Read Out Mode Setup; anything
///         other than 0, denotes an error and a corresponding error message
///         will be written to stderr.
int setup_read_out_mode(const AndorParameters *params) noexcept {

  char buf[32] = {'\0'}; /* buffer to store datetime string */
  unsigned int status;
  int irom = ReadOutMode2int(params->read_out_mode_);
  printf("[DEBUG][%s] Setting ReadOutMode to %1d\n", date_str(buf), irom);

  switch (params->read_out_mode_) {

  /* Full Vertical Binning */
  case ReadOutMode::FullVerticalBinning:
    status = SetReadMode(irom);
    break;

  /* Single Track */
  case ReadOutMode::SingleTrack:
    if (status = SetReadMode(irom); status == DRV_SUCCESS) {
      printf(
          "[DEBUG][%s] Setting up SingleTrack Acquisition Mode, center: %5d, "
          "height: %5d\n",
          date_str(buf), params->singe_track_center_,
          params->single_track_height_);
      status = SetSingleTrack(params->singe_track_center_,
                              params->single_track_height_);
    }
    break;

  /* Multi Track */
  case ReadOutMode::MultiTrack:
    fprintf(stderr,
            "[ERROR][%s] MultiTrack Read-Out Mode is not applicable; need "
            "more code! (traceback: %s)\n",
            date_str(buf), __func__);
    status = std::numeric_limits<unsigned int>::max();
    break;

  /* Random Track */
  case ReadOutMode::RandomTrack:
    fprintf(stderr,
            "[ERROR][%s] Random-Track Read-Out Mode is not applicable; "
            "need more code! (traceback: %s)\n",
            date_str(buf), __func__);
    status = std::numeric_limits<unsigned int>::max();
    break;

  case ReadOutMode::Image:
    if (status = SetReadMode(irom); status == DRV_SUCCESS) {
      printf(
          "[DEBUG][%s] Setting up Image Acquisition Mode, with parameters:\n",
          date_str(buf));
      printf("[DEBUG][%s]           Horizontal Vertical\n", buf);
      printf("[DEBUG][%s] %10s %10d %8d\n", "binning", buf, params->image_hbin_,
             params->image_vbin_);
      printf("[DEBUG][%s] %10s %10d %8d\n", "start pix.", buf,
             params->image_hstart_, params->image_vstart_);
      printf("[DEBUG][%s] %10s %10d %8d\n", "end pix.", buf,
             params->image_hend_, params->image_vend_);
      status = SetImage(params->image_hbin_, params->image_vbin_,
                        params->image_hstart_, params->image_hend_,
                        params->image_vstart_, params->image_vend_);
    }
    break;

  default:
    fprintf(
        stderr,
        "[ERROR][%s] No valid ReadOut Mode was specified! (traceback: %s)\n",
        date_str(buf), __func__);
    status = std::numeric_limits<unsigned int>::max();
  } /* done setting up read out parameters */

  /* check status of parameter setting; report errors */
  auto retcode = status;
  switch (status) {

  case DRV_SUCCESS:
    retcode = 0;
    break;

  case DRV_NOT_INITIALIZED:
    fprintf(stderr,
            "[ERROR][%s] Failed to set Read Mode; system not initialized! "
            "(traceback: %s)\n",
            date_str(buf), __func__);
    retcode = 1;
    break;

  case DRV_ACQUIRING:
    fprintf(stderr,
            "[ERROR][%s] Failed to set Read Mode; acquisition in progress! "
            "(traceback: %s)\n",
            date_str(buf), __func__);
    retcode = 2;
    break;

  case DRV_P1INVALID:
    if (params->read_out_mode_ == ReadOutMode::FullVerticalBinning) {
      fprintf(stderr,
              "[ERROR][%s] Failed to set Read mode; invalid ReadOutMode given! "
              "(traceback: %s)\n",
              date_str(buf), __func__);
    } else if (params->read_out_mode_ == ReadOutMode::SingleTrack) {
      fprintf(stderr,
              "[ERROR][%s] Failed to set Read Mode; center row invalid! "
              "(traceback: %s)\n",
              date_str(buf), __func__);
    } else if (params->read_out_mode_ == ReadOutMode::Image) {
      fprintf(stderr,
              "[ERROR][%s] Failed to set Read Mode; binning parameter invalid! "
              "(traceback: %s)\n",
              date_str(buf), __func__);
    }
    retcode = 3;
    break;

  case DRV_P2INVALID:
    if (params->read_out_mode_ == ReadOutMode::SingleTrack) {
      fprintf(stderr,
              "[ERROR][%s] Failed to set Read Mode; Track height invalid! "
              "(traceback: %s)\n",
              date_str(buf), __func__);
    } else if (params->read_out_mode_ == ReadOutMode::Image) {
      fprintf(stderr,
              "[ERROR][%s] Failed to set Read Mode; binning parameter invalid! "
              "(traceback: %s)\n",
              date_str(buf), __func__);
    }
    retcode = 4;
    break;

  case DRV_P3INVALID:
    [[fallthrough]];
  case DRV_P4INVALID:
    [[fallthrough]];
  case DRV_P5INVALID:
    [[fallthrough]];
  case DRV_P6INVALID:
    fprintf(stderr,
            "[ERROR][%s] Failed to set Read Mode; Sub-area "
            "co-ordinate is invalid (traceback: %s)\n",
            date_str(buf), __func__);
    retcode = 6;
    break;

  default:
    fprintf(stderr,
            "[ERROR][%s] Failed to set Read mode; undocumented error! "
            "(traceback: %s)\n",
            date_str(buf), __func__);
    retcode = 10;
  }

  return retcode;
}