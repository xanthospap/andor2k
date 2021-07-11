#include "andor2k.hpp"
#include "atmcdLXd.h"
#include <cstdio>
#include <limits>

int setup_read_out_mode(const AndorParameters *params) noexcept {
  char buf[32] = {'\0'};
  unsigned int status;

  int irom = ReadOutMode2int(params->read_out_mode_);
  printf("[DEBUG][%s] Setting ReadOutMode to %1d\n", date_str(buf), irom);

  switch (params->read_out_mode_) {
  case ReadOutMode::FullVerticalBinning:
    status = SetReadMode(irom);
    break;

  case ReadOutMode::SingleTrack:
    if (status = SetReadMode(irom); status == DRV_SUCCESS) {
      printf("[DEBUG][%s] Setting up SingleTrack Acquisition Mode, center: %5d, "
             "height: %5d\n",date_str(buf),
             params->singe_track_center_, params->single_track_height_);
      status = SetSingleTrack(params->singe_track_center_,
                              params->single_track_height_);
    }
    break;

  case ReadOutMode::MultiTrack:
    fprintf(stderr, "[ERROR][%s] MultiTrack Read-Out Mode is not applicable; need "
                    "more code!\n", date_str(buf));
    status = std::numeric_limits<unsigned int>::max();
    break;

  case ReadOutMode::RandomTrack:
    fprintf(stderr, "[ERROR][%s] Random-Track Read-Out Mode is not applicable; "
                    "need more code!\n",date_str(buf));
    status = std::numeric_limits<unsigned int>::max();
    break;

  case ReadOutMode::Image:
    if (status = SetReadMode(irom); status == DRV_SUCCESS) {
      printf("[DEBUG][%s] Setting up Image Acquisition Mode, with parameters:\n", date_str(buf));
      printf("[DEBUG][%s]           Horizontal Vertical\n", buf);
      printf("[DEBUG][%s] %10s %10d %8d\n", "binning", buf, params->image_hbin_,
             params->image_vbin_);
      printf("[DEBUG][%s] %10s %10d %8d\n", "start pix.", buf, params->image_hstart_,
             params->image_vstart_);
      printf("[DEBUG][%s] %10s %10d %8d\n", "end pix.", buf, params->image_hend_,
             params->image_vend_);
      status = SetImage(params->image_hbin_, params->image_vbin_,
                        params->image_hstart_, params->image_hend_,
                        params->image_vstart_, params->image_vend_);
    }
    break;

  default:
    fprintf(stderr, "[ERROR][%s] No valid ReadOut Mode was specified!\n", date_str(buf));
    status = std::numeric_limits<unsigned int>::max();
  }

  switch (status) {
  case DRV_SUCCESS:
    printf("[DEBUG][%s] ReadOut Mode set successfully.\n", date_str(buf));
    return 0;
  case DRV_NOT_INITIALIZED:
    fprintf(stderr,
            "[ERROR][%s] Failed to set Read Mode; system not initialized!\n", date_str(buf));
    return 1;
  case DRV_ACQUIRING:
    fprintf(stderr,
            "[ERROR][%s] Failed to set Read Mode; acquisition in progress!\n", date_str(buf));
    return 2;
  case DRV_P1INVALID:
    if (params->read_out_mode_ == ReadOutMode::FullVerticalBinning) {
      fprintf(stderr,
              "[ERROR][%s] Failed to set Read mode; invalid ReadOutMode given!\n", date_str(buf));
      return 3;
    } else if (params->read_out_mode_ == ReadOutMode::SingleTrack) {
      fprintf(stderr, "[ERROR][%s] Failed to set Read Mode; center row invalid!\n", date_str(buf));
      return 3;
    } else if (params->read_out_mode_ == ReadOutMode::Image) {
      fprintf(stderr,
              "[ERROR][%s] Failed to set Read Mode; binning parameter invalid!\n", date_str(buf));
      return 3;
    }
    return 100;
  case DRV_P2INVALID:
    if (params->read_out_mode_ == ReadOutMode::SingleTrack) {
      fprintf(stderr,
              "[ERROR][%s] Failed to set Read Mode; Track height invalid!\n", date_str(buf));
      return 4;
    } else if (params->read_out_mode_ == ReadOutMode::Image) {
      fprintf(stderr,
              "[ERROR][%s] Failed to set Read Mode; binning parameter invalid!\n", date_str(buf));
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
    fprintf(stderr, "[ERROR][%s] Failed to set Read Mode; Sub-area "
                    "co-ordinate is invalid\n", date_str(buf));
    return 6;
  default:
    fprintf(stderr, "[ERROR][%s] Failed to set Read mode; undocumented error!\n", date_str(buf));
    return 5;
  }
}