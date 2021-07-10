#include "andor2k.hpp"
#include "atmcdLXd.h"
#include <cstdio>
#include <limits>

int setup_read_out_mode(const CmdParameters *params) noexcept {
  unsigned int status;
  int irom = ReadOutMode2int(params->read_out_mode_);
  printf("[DEBUG] Setting ReadOutMode to %1d\n", irom);

  switch (params->read_out_mode_) {
  case ReadOutMode::FullVerticalBinning:
    status = SetReadMode(irom);
    break;

  case ReadOutMode::SingleTrack:
    if (status = SetReadMode(irom); status == DRV_SUCCESS) {
      printf("[DEBUG] Setting up SingleTrack Acquisition Mode, center: %5d, "
             "height: %5d\n",
             params->singe_track_center_, params->single_track_height_);
      status = SetSingleTrack(params->singe_track_center_,
                              params->single_track_height_);
    }
    break;

  case ReadOutMode::MultiTrack:
    fprintf(stderr, "[ERROR] MultiTrack Read-Out Mode is not applicable; need "
                    "more code!\n");
    status = std::numeric_limits<unsigned int>::max();
    break;

  case ReadOutMode::RandomTrack:
    fprintf(stderr, "[ERROR] Random-Track Read-Out Mode is not applicable; "
                    "need more code!\n");
    status = std::numeric_limits<unsigned int>::max();
    break;

  case ReadOutMode::Image:
    if (status = SetReadMode(irom); status == DRV_SUCCESS) {
      printf("[DEBUG] Setting up Image Acquisition Mode, with parameters:\n");
      printf("[DEBUG]           Horizontal Vertical\n");
      printf("[DEBUG] %10s %10d %8d\n", "binning", params->image_hbin_,
             params->image_vbin_);
      printf("[DEBUG] %10s %10d %8d\n", "start pix.", params->image_hstart_,
             params->image_vstart_);
      printf("[DEBUG] %10s %10d %8d\n", "end pix.", params->image_hend_,
             params->image_vend_);
      status = SetImage(params->image_hbin_, params->image_vbin_,
                        params->image_hstart_, params->image_hend_,
                        params->image_vstart_, params->image_vend_);
    }
    break;

  default:
    fprintf(stderr, "[ERROR] No valid ReadOut Mode was specified!\n");
    status = std::numeric_limits<unsigned int>::max();
  }

  switch (status) {
  case DRV_SUCCESS:
    printf("[DEBUG] ReadOut Mode set successfully.\n");
    return 0;
  case DRV_NOT_INITIALIZED:
    fprintf(stderr,
            "[ERROR] Failed to set Read Mode; system not initialized!\n");
    return 1;
  case DRV_ACQUIRING:
    fprintf(stderr,
            "[ERROR] Failed to set Read Mode; acquisition in progress!\n");
    return 2;
  case DRV_P1INVALID:
    if (params->read_out_mode_ == ReadOutMode::FullVerticalBinning) {
      fprintf(stderr,
              "[ERROR] Failed to set Read mode; invalid ReadOutMode given!\n");
      return 3;
    } else if (params->read_out_mode_ == ReadOutMode::SingleTrack) {
      fprintf(stderr, "[ERROR] Failed to set Read Mode; center row invalid!\n");
      return 3;
    } else if (params->read_out_mode_ == ReadOutMode::Image) {
      fprintf(stderr,
              "[ERROR] Failed to set Read Mode; binning parameter invalid!\n");
      return 3;
    }
    return 100;
  case DRV_P2INVALID:
    if (params->read_out_mode_ == ReadOutMode::SingleTrack) {
      fprintf(stderr,
              "[ERROR] Failed to set Read Mode; Track height invalid!\n");
      return 4;
    } else if (params->read_out_mode_ == ReadOutMode::Image) {
      fprintf(stderr,
              "[ERROR] Failed to set Read Mode; binning parameter invalid!\n");
      return 4;
    }
    return 100;
  case DRV_P3INVALID:
    [[fallthrough]];
  case DRV_P4INVALID:
    [[fallthrough]];
  case DRV_P5INVALID:
    [[fallthrough]];
  case DRV_P6INVALID:
    fprintf(stderr, "[ERROR] Failed to set Read Mode; Sub-area "
                    "co-ordinate is invalid\n");
    return 6;
  default:
    fprintf(stderr, "[ERROR] Failed to set Read mode; undocumented error!\n");
    return 5;
  }
}