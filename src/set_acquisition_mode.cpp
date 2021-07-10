#include "andor2k.hpp"
#include "atmcdLXd.h"
#include <cstdio>
#include <limits>

int setup_acquisition_mode(const AndorParameters *params) noexcept {
  unsigned int status;
  int imode = AcquisitionMode2int(params->acquisition_mode_);

  switch (params->acquisition_mode_) {
  case AcquisitionMode::SingleScan:
    status = SetAcquisitionMode(imode);
    if (status == DRV_SUCCESS) {
      // set the exposure time
      status = SetExposureTime(params->exposure_);
    }
    break;

  case AcquisitionMode::Accumulate:
    status = SetAcquisitionMode(imode);
    if (status == DRV_SUCCESS) {
      // set the exposure time
      if (status = SetExposureTime(params->exposure_); status == DRV_SUCCESS) {
        // set the number of scans accumulated in memory
        if (status = SetNumberAccumulations(params->num_accumulations_);
            status == DRV_SUCCESS) {
          // set the accumulation cycle time to the nearest valid value not less
          // than the given value (seconds)
          status = SetAccumulationCycleTime(params->accumulation_cycle_time_);
        }
      }
    }
    break;

  case AcquisitionMode::KineticSeries:
    status = SetAcquisitionMode(imode);
    if (status == DRV_SUCCESS) {
      // set the exposure time
      if (status = SetExposureTime(params->exposure_); status == DRV_SUCCESS) {
        // set the number of scans accumulated in memory
        if (status = SetNumberAccumulations(params->num_accumulations_);
            status == DRV_SUCCESS) {
          // set the accumulation cycle time to the nearest valid value not less
          // than the given value (seconds)
          if (status =
                  SetAccumulationCycleTime(params->accumulation_cycle_time_);
              status == DRV_SUCCESS) {
            // set the number of scans (possibly accumulated scans) to be taken
            // during a single acquisition sequence
            if (status = SetNumberKinetics(params->num_images_);
                status == DRV_SUCCESS) {
              // set the kinetic cycle time to the nearest valid value not less
              // than the given value
              status = SetKineticCycleTime(params->kinetics_cycle_time_);
            }
          }
        }
      }
    }
    break;

  case AcquisitionMode::RunTillAbort:
    status = SetAcquisitionMode(imode);
    if (status == DRV_SUCCESS) {
      // set the exposure time
      if (status = SetExposureTime(params->exposure_); status == DRV_SUCCESS) {
        // set the kinetic cycle time to the nearest valid value not less than
        // the given value
        status = SetKineticCycleTime(params->kinetics_cycle_time_);
      }
    }
    break;

  default:
    fprintf(stderr, "[ERROR] Acquisition Mode is not applicable!\n");
    status = std::numeric_limits<unsigned int>::max();
  }

  switch (status) {
  case DRV_SUCCESS:
    return 0;
  case DRV_NOT_INITIALIZED:
    fprintf(
        stderr,
        "[ERROR] Failed to set Acquisition Mode; system not initialized!\n");
    return 1;
  case DRV_ACQUIRING:
    fprintf(
        stderr,
        "[ERROR] Failed to set Acquisition Mode; acquisition in progress\n");
    return 2;
  case DRV_P1INVALID:
    fprintf(stderr,
            "[ERROR] Failed to set Acquisition Mode; invalid mode parameter\n");
    return 3;
  default:
    fprintf(stderr,
            "[ERROR] Failed to set Acquisition Mode; undocumented error!\n");
    return 5;
  }
}