#include "andor2k.hpp"
#include "atmcdLXd.h"
#include <cstdio>
#include <limits>

/// @bief Set AcquisitionMode for Andor2k camera
/// @see USER’S GUIDE TO SDK, Software Version 2.102
///
/// This function will set the Acquisition Mode in the ANDOR2K camera. The 
/// Acquisition Mode to set, will be read off from the input params instance.
/// All other parameters needed to setup the AcquisitionMode (e.g. exposure 
/// time), will also be read off from the params input instance.
///
/// Possible acquistion modes, are:
/// * Single Scan => (int) 1
///   setup AcquisitionMode to 1
///   setup ExposureTime
/// * Accumulate => (int) 2
///   setup AcquisitionMode to 2
///   setup ExposureTime
///   setup Number of Accumulations
///   setup Accumulated Cycle Time
/// * Kinetic Series => (int) 3
///   setup AcquisitionMode to 3
///   setup ExposureTime
///   setup Number of Accumulations
///   setup Accumulate Cycle Time
///   setup Number in Kinetic Series
///   setup Kinetic Cycle Time
/// * Run Till Abort => (int) 5
///   setup AcquisitionMode to 5
///   setup ExposureTime
///   setup Kinetic Cycle Time
/// * Fast Kinetics
///   Mode not available; will trigger an error
///
/// @param[in] params An instance of type AndorParameters; the function will set
///            all needed parameters based on values stored in this instance
///            (including AcquisitionMode, exposure time, etc).
///
/// @return An integer deonting the status of the AcquisitionSetup; anything
///         other than 0, denotes an error and a corresponding error message
///         will be written to stderr.
int setup_acquisition_mode(const AndorParameters *params) noexcept {
  
  unsigned int status;
  char buf[32] = {'\0'}; /* buffer to hold datetime string for reporting */
  int imode = AcquisitionMode2int(params->acquisition_mode_);
  printf("[DEBUG][%s] Setting AcquisitionMode to %1d\n", date_str(buf), imode);

  switch (params->acquisition_mode_) {
  
  /// SinlgeScan
  case AcquisitionMode::SingleScan:
    status = SetAcquisitionMode(imode);
    if (status == DRV_SUCCESS) {
      status = SetExposureTime(params->exposure_); /* set exposure time */
    }
    break;

  /// Accumulate
  case AcquisitionMode::Accumulate:
    status = SetAcquisitionMode(imode);
    if (status == DRV_SUCCESS) {
      /* set the exposure time */
      if (status = SetExposureTime(params->exposure_); status == DRV_SUCCESS) {
        /* set the number of scans accumulated in memory */
        if (status = SetNumberAccumulations(params->num_accumulations_);
            status == DRV_SUCCESS) {
          /* set the accumulation cycle time to the nearest valid value not less
             than the given value (seconds) */
          status = SetAccumulationCycleTime(params->accumulation_cycle_time_);
        }
      }
    }
    break;

  /// Kinetic Series
  case AcquisitionMode::KineticSeries:
    status = SetAcquisitionMode(imode);
    if (status == DRV_SUCCESS) {
      /* set the exposure time */
      if (status = SetExposureTime(params->exposure_); status == DRV_SUCCESS) {
        /* set the number of scans accumulated in memory */
        if (status = SetNumberAccumulations(params->num_accumulations_);
            status == DRV_SUCCESS) {
          /* set the accumulation cycle time to the nearest valid value not less
             than the given value (seconds) */
          if (status =
                  SetAccumulationCycleTime(params->accumulation_cycle_time_);
              status == DRV_SUCCESS) {
            /* set the number of scans (possibly accumulated scans) to be taken
               during a single acquisition sequence */
            if (status = SetNumberKinetics(params->num_images_);
                status == DRV_SUCCESS) {
              /* set the kinetic cycle time to the nearest valid value not less
                 than the given value */
              status = SetKineticCycleTime(params->kinetics_cycle_time_);
            }
          }
        }
      }
    }
    break;

  /// Run Till Abort
  case AcquisitionMode::RunTillAbort:
    status = SetAcquisitionMode(imode);
    if (status == DRV_SUCCESS) {
      /* set the exposure time */
      if (status = SetExposureTime(params->exposure_); status == DRV_SUCCESS) {
        /* set the kinetic cycle time to the nearest valid value not less than
           the given value */
        status = SetKineticCycleTime(params->kinetics_cycle_time_);
      }
    }
    break;

  default:
    fprintf(stderr, "[ERROR][%s] Acquisition Mode is not applicable! (traceback: %s)\n",
            date_str(buf), __func__);
    status = std::numeric_limits<unsigned int>::max();
  } /* done setting parameters */

  /* Check the status of parameter setting done above */
  switch (status) {
  case DRV_SUCCESS:
    return 0;
  
  case DRV_NOT_INITIALIZED:
    fprintf(
        stderr,
        "[ERROR][%s] Failed to set Acquisition Mode; system not initialized! (traceback: %s)\n",
        date_str(buf), __func__);
    return 1;
  
  case DRV_ACQUIRING:
    fprintf(
        stderr,
        "[ERROR][%s] Failed to set Acquisition Mode; acquisition in progress (traceback: %s)\n",
        date_str(buf), __func__);
    return 2;
  
  case DRV_P1INVALID:
    fprintf(
        stderr,
        "[ERROR][%s] Failed to set Acquisition Mode; invalid mode parameter (traceback: %s)\n",
        date_str(buf), __func__);
    return 3;
  
  default:
    fprintf(stderr,
            "[ERROR][%s] Failed to set Acquisition Mode; undocumented error! (traceback: %s)\n",
            date_str(buf), __func__);
    return 5;
  }
}