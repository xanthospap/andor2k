#include "andor2k.hpp"

int coarse_exposure_time(const AndorParameters* params, long &millisec_per_image, long &total_millisec) noexcept {
    float exposure, accumulate, kinetic, readouttime;
    
    // get actual valid acquisition timings
    if (unsigned error = GetAcquisitionTimings(&exposure, &accumulate, &kinetic); error != DRV_SUCCESS) {
        return 1;
    }

    // get readout time for a single acquisition
    if (unsigned error = GetReadOutTime(&readouttime); error != DRV_SUCCESS) {
        return 1;
    }
    
    long exposure_ms = static_cast<long>(exposure * 1e3);
    long accumulate_ms = static_cast<long>(accumulate *1e3);
    long kinetic_ms = static_cast<long>(kinetic *1e3);
    long readouttime_ms = static_cast<long>(readouttime * 1e3);

    // compute time to fully acquire (aka expose plus readout) a signle image
    switch (params->acquisition_mode_) {
        case AcquisitionMode::SingleScan:
            millisec_per_image = exposure_ms + readouttime_ms;
            total_millisec = params->num_images_ * millisec_per_image;
            break;
        
        case AcquisitionMode::Accumulate:
            total_millisec = (params->num_accumulations_ - 1) * (accumulate_ms - exposure_ms) + exposure_ms + readouttime_ms;
            millisec_per_image = total_millisec;
            break;

        case AcquisitionMode::KineticSeries:
            millisec_per_image = (params->num_accumulations_ - 1) * (accumulate_ms - exposure_ms) + exposure_ms + readouttime_ms;
            total_millisec = (params->num_images_ -1) * kinetic_ms + millisec_per_image;
            break;
        
        case AcquisitionMode::RunTillAbort:
            millisec_per_image = exposure_ms + readouttime_ms;
            total_millisec = params->num_images_ * millisec_per_image;
            break;
        
        default:
            return 10;
    }
   
    return 0;
}
