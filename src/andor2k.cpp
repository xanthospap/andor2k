#include "andor2k.hpp"
#include "atmcdLXd.h"
#include <chrono>
#include <cmath>
#include <cstdio>
#include <fstream>
#include <thread>

using namespace std::chrono_literals;

char fits_file[256] = {'\0'};

int main(int argc, char *argv[]) {

  unsigned long error;

  // command line options
  AndorParameters params;

  // resolve command line parameters
  if (resolve_cmd_parameters(argc, argv, params)) {
    fprintf(stderr, "[FATAL] Failed to resolve cmd parameters...exiting\n");
    return 10;
  }

  // select the camera
  if (select_camera(params.camera_num_) < 0) {
    fprintf(stderr, "[FATAL] Failed to select camera...exiting\n");
    return 10;
  }

  // initialize CCD
  printf("[DEBUG] Initializing CCD ....");
  error = Initialize(params.initialization_dir_);
  if (error != DRV_SUCCESS) {
    fprintf(stderr, "[FATAL] Initialisation error...exiting\n");
    return 10;
  }
  // allow initialization ... go to sleep for two seconds
  std::this_thread::sleep_for(2000ms);
  printf("... ok!\n");

  // set read mode
  if (setup_read_out_mode(&params)) {
    fprintf(stderr, "[FATAL] Failed to set read mode...exiting\n");
    return 10;
  }

  // set acquisition mode (this will also set the exposure time)
  if (setup_acquisition_mode(&params)) {
    fprintf(stderr, "[FATAL] Failed to set acquisition mode...exiting\n");
    return 10;
  }
  // get the current "valid" acquisition timing information
  if (params.acquisition_mode_ != AcquisitionMode::SingleScan) {
    float vexposure, vaccumulate, vkinetic;
    if (GetAcquisitionTimings(&vexposure, &vaccumulate, &vkinetic) !=
        DRV_SUCCESS) {
      fprintf(stderr, "[FATAL] Failed to get current valid acquisition "
                      "timings...exiting\n");
      return 10;
    }
    printf("[DEBUG] Actual acquisition timings tuned by ANDOR:\n");
    printf("[DEBUG] Exposure Time        : %5.2f sec. (vs %5.2f given)\n",
           vexposure, params.exposure_);
    printf("[DEBUG] Accumulate Cycle Time: %5.2f sec. (vs %5.2f given)\n",
           vaccumulate, params.accumulation_cycle_time_);
    printf("[DEBUG] Kinetic Cycle Time   : %5.2f sec. (vs %5.2f given)\n",
           vkinetic, params.kinetics_cycle_time_);
  }

  // get size of the detector in pixels
  int xpixels, ypixels;
  error = GetDetector(&xpixels, &ypixels);
  if (error != DRV_SUCCESS) {
    fprintf(stderr, "[FATAL] Failed to get detector dimensions...exiting\n");
    return 10;
  }
  printf("[DEBUG] Detector dimensions: %5dx%5d (in pixels)\n", xpixels,
         ypixels);
  printf("[DEBUG] Computed values are:\n");
  int width = params.image_hend_ - params.image_hstart_ + 1,
      height = params.image_vend_ - params.image_vstart_ + 1;
  long pixels = (width / params.image_hbin_) * (height / params.image_vbin_);
  printf("[DEBUG] Width  = %5d - %5d = %5d\n", params.image_hend_,
         params.image_hstart_, width);
  printf("[DEBUG] Height = %5d - %5d = %5d\n", params.image_vend_,
         params.image_vstart_, height);
  printf("[DEBUG] Num of pixels: %10ld\n", pixels);

  // initialize shutter
  error =
      SetShutter(1, ShutterMode2int(params.shutter_mode_),
                 params.shutter_closing_time_, params.shutter_opening_time_);
  if (error != DRV_SUCCESS) {
    fprintf(stderr, "[FATAL] Failed to initialize shutter...exiting\n");
    return 10;
  }
  printf("[DEBUG] Shutter initialized to closing/opening delay: %3d/%3d "
         "milliseconds\n",
         params.shutter_closing_time_, params.shutter_opening_time_);
  printf("[DEBUG] Shutter mode : %1d\n", ShutterMode2int(params.shutter_mode_));

  // cool down if needed
  if (cool_to_temperature(&params)) {
    fprintf(stderr, "[FATAL] Failed to set target temperature...exiting\n");
    return 10;
  }

  // start acquisition ...
  /* ------------------------------------------------------------ ACQUISITION */
  printf("[DEBUG] Starting image acquisition ...\n");
  StartAcquisition();
  int status;
  if (params.acquisition_mode_ == AcquisitionMode::SingleScan) {
    at_32 *imageData = new at_32[xpixels * ypixels];
    std::fstream fout("image.txt", std::ios::out);
    // Loop until acquisition finished
    GetStatus(&status);
    while (status == DRV_ACQUIRING)
      GetStatus(&status);
    GetAcquiredData(imageData, xpixels * ypixels);
    for (int i = 0; i < xpixels * ypixels; i++)
      fout << imageData[i] << std::endl;
    // SaveAsBmp("./image.bmp", "./GREY.PAL", 0, 0);
    if (get_next_fits_filename(&params, fits_file)) {
      fprintf(stderr,
              "[ERROR] Failed getting FITS filename! No FITS image saved\n");
    }
    printf("[DEBUG] Image acquired; saving to FITS file \"%s\" ...", fits_file);
    if (SaveAsFITS(fits_file, 3) != DRV_SUCCESS) {
      fprintf(stderr, "\n[ERROR] Failed to save image to fits format!\n");
    } else {
      printf(" done!\n");
    }
    delete[] imageData;

  } else if (params.acquisition_mode_ == AcquisitionMode::KineticSeries) {
    at_32 *data = new at_32[xpixels * ypixels];
    int images_acquired = 0;
    bool abort = false;
    auto sleep_ms = std::chrono::milliseconds(
        static_cast<int>(std::ceil(params.exposure_ + 0.5)) * 1000);
    std::this_thread::sleep_for(sleep_ms);
    while ((images_acquired < params.num_images_) && !abort) {
      error = GetOldestImage(data, xpixels * ypixels);
      switch (error) {
      case DRV_SUCCESS:
        ++images_acquired;
        printf("[DEBUG] Acquired image nr %2d/%2d\n", images_acquired,
               params.num_images_);
        if (get_next_fits_filename(&params, fits_file)) {
          fprintf(
              stderr,
              "[ERROR] Failed getting FITS filename! No FITS image saved\n");
        }
        printf("[DEBUG] Image acquired; saving to FITS file \"%s\" ...",
               fits_file);
        if (SaveAsFITS(fits_file, 3) != DRV_SUCCESS) {
          fprintf(stderr, "\n[ERROR] Failed to save image to fits format!\n");
        } else {
          printf(" done!\n");
        }
        break;
      case DRV_NOT_INITIALIZED:
        fprintf(stderr,
                "[ERROR] Failed to acquire image; system not initialized\n");
        [[fallthrough]];
      case DRV_ERROR_ACK:
        fprintf(stderr, "[ERROR] Failed to acquire image; unable to "
                        "communicate with card\n");
        [[fallthrough]];
      case DRV_P1INVALID:
        fprintf(
            stderr,
            "[ERROR] Failed to acquire image; data array size is incorrect\n");
        AbortAcquisition();
        abort = true;
        break;
      case DRV_NO_NEW_DATA:
        printf("[DEBUG] No new data yet ... waiting ...\n");
        std::this_thread::sleep_for(sleep_ms);
      }
    }
    delete[] data;
  }
  /* ------------------------------------------------------------ ACQUISITION */

  // system shutdown
  system_shutdown(&params);

  return 0;
}