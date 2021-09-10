#include "andor2k.hpp"
#include <cstring>

int sig_abort_set = 0;
int sig_interrupt_set = 0;
int abort_exposure_set = 0;
int stop_reporting_thread = 0;
int acquisition_thread_finished = 0;

void AndorParameters::set_defaults() noexcept {
  camera_num_ = 0;
  exposure_ = 0.1;
  num_images_ = 1;
  std::memset(type_, 0, MAX_IMAGE_TYPE_CHARS);
  std::strcpy(initialization_dir_, "/usr/local/etc/andor");
  std::memset(image_filename_, 0, MAX_FITS_FILENAME_SIZE);
  std::strcpy(save_dir_, "/home/andor2k/fits");
  std::strcpy(object_name_, " ");
  read_out_mode_ = ReadOutMode::Image;
  singe_track_center_ = 1;
  single_track_height_ = 1;
  image_hbin_ = image_vbin_ = image_hstart_ = image_vstart_ = 1;
  image_hend_ = image_vend_ = MAX_PIXELS_IN_DIM;
  acquisition_mode_ = AcquisitionMode::SingleScan;
  num_accumulations_ = 1;
  accumulation_cycle_time_ = 0.2;
  kinetics_cycle_time_ = 0.5;
  shutter_mode_ = ShutterMode::FullyAuto;
  shutter_closing_time_ = 50;
  shutter_opening_time_ = 50;
  cooler_mode_ = 0;
  ar_hdr_tries_ = 0;
}

char *get_status_string(char *buffer) noexcept {
  int status;
  GetStatus(&status);

  std::memset(buffer, 0, MAX_STATUS_STRING_SIZE);

  switch (status) {
  case DRV_IDLE:
    std::strcpy(buffer, "IDLE; waiting for instructions");
    break;
  case DRV_TEMPCYCLE:
    std::strcpy(buffer, "Executing temperature cycle");
    break;
  case DRV_ACQUIRING:
    std::strcpy(buffer, "Acquisition in progress");
    break;
  case DRV_ACCUM_TIME_NOT_MET:
    std::strcpy(buffer, "Unable to meet Accumulate cycle time");
    break;
  case DRV_KINETIC_TIME_NOT_MET:
    std::strcpy(buffer, "Unable to meet Kinetic cycle time");
    break;
  case DRV_ERROR_ACK:
    std::strcpy(buffer, "Unable communicate with card");
    break;
  case DRV_ACQ_BUFFER:
    std::strcpy(buffer, "Computer unable to read the data via the ISA slot at "
                        "the required rate");
    break;
  case DRV_ACQ_DOWNFIFO_FULL:
    std::strcpy(buffer, "Computer unable to read data fast enough to stop "
                        "camera memory going full");
    break;
  case DRV_SPOOLERROR:
    std::strcpy(buffer, "Overflow of the spool buffer");
    break;
  default:
    std::strcpy(buffer, "Camera in unknown/undocumented state!");
    break;
  }
  return buffer;
}

char *get_start_acquisition_status_string(unsigned int error,
                                          char *buffer) noexcept {

  std::memset(buffer, 0, MAX_STATUS_STRING_SIZE);

  switch (error) {
  case DRV_SUCCESS:
    std::strcpy(buffer, "Acquisition started");
    break;
  case DRV_NOT_INITIALIZED:
    std::strcpy(buffer, "System not initialized");
    break;
  case DRV_ACQUIRING:
    std::strcpy(buffer, "Acquisition in progess");
    break;
  case DRV_VXDNOTINSTALLED:
    std::strcpy(buffer, "VxX not loaded");
    break;
  case DRV_ERROR_ACK:
    std::strcpy(buffer, "Unable to communicate with card");
    break;
  case DRV_INIERROR:
    std::strcpy(buffer, "Error reading \'DETECTOR.INI\'");
    break;
    // case DRV_ACQERROR:
    //   std::strcpy(buffer, "Acquisition settings invalid");
    //   break;
  case DRV_ERROR_PAGELOCK:
    std::strcpy(buffer, "Unable to allocate memory");
    break;
  case DRV_INVALID_FILTER:
    std::strcpy(buffer, "Filter not available for current acquisition");
    break;
  case DRV_BINNING_ERROR:
    std::strcpy(buffer, "Range not multiple of horizontal binning");
    break;
  case DRV_SPOOLSETUPERROR:
    std::strcpy(buffer, "Error with spool settings");
    break;
  default:
    std::strcpy(buffer, "unknown/undocumented acquisition state!");
    break;
  }
  return buffer;
}