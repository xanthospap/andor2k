#ifndef __HELMOS_ANDOR2K_HPP__
#define __HELMOS_ANDOR2K_HPP__

#include "atmcdLXd.h"
#include "fits_header.hpp"
#include "cpp_socket.hpp"
#include <cstdint>
#include <limits>

// @brief Max chars for any fits filename (excluding path)
constexpr int MAX_FITS_FILENAME_SIZE = 128;

/// @brief Max chars for image type (aka flat, bias, etc)
constexpr int MAX_IMAGE_TYPE_CHARS = 16;

constexpr int MAX_SOCKET_BUFFER_SIZE = 1024;

// @brief Max chars for any fits file (including path)
constexpr int MAX_FITS_FILE_SIZE = 256;

constexpr int MAX_STATUS_STRING_SIZE = 128;

constexpr int MAX_OBJECT_NAME_CHARS = 32;

constexpr int MAX_FILTER_NAME_CHARS = 16;

/// @brief Minimum temperature to reach before shut down
constexpr int SHUTDOWN_TEMPERATURE = 2;

/// @brief Max minutes to wait for when shuting down camera (to reach shutdown
///        temperature).
constexpr int MAX_SHUTDOWN_DURATION = 30;

/// @brief Max minutes to wait for when cooling down camera (to reach given
///        temperature).
constexpr int MAX_COOLING_DURATION = 30;

/// @brief Max pixels in width/height
constexpr int MAX_PIXELS_IN_DIM = 2048;

constexpr int ABORT_EXIT_STATUS = std::numeric_limits<int>::max();

constexpr int INTERRUPT_EXIT_STATUS = std::numeric_limits<int>::max();

enum class ReadOutMode : int_fast8_t {
  FullVerticalBinning = 0,
  MultiTrack = 1,
  RandomTrack = 2,
  SingleTrack = 3,
  Image = 4
}; // ReadOutModes

enum class AcquisitionMode : int_fast8_t {
  SingleScan = 1,
  Accumulate = 2,
  KineticSeries = 3,
  FastKinetics = 4,
  RunTillAbort = 5
}; // AcquisitionMode

enum class ShutterMode : int_fast8_t {
  FullyAuto = 0,
  PermanentlyOpen,
  PermanentlyClosed,
  OpenForFVBSeries,
  OpenForAnySeries
}; // ShutterMode

struct AndorParameters {
  void set_defaults() noexcept;

  int camera_num_{0};
  float exposure_;
  int num_images_{1};

  char initialization_dir_[128] = "/usr/local/etc/andor";
  char save_dir_[128] = "/home/andor2k/fits";
  char type_[MAX_IMAGE_TYPE_CHARS];
  char image_filename_[MAX_FITS_FILENAME_SIZE];
  char object_name_[MAX_OBJECT_NAME_CHARS];
  char filter_name_[MAX_FILTER_NAME_CHARS];

  /* options for read-out mode */
  ReadOutMode read_out_mode_{ReadOutMode::Image};
  // Single Track Mode
  int singe_track_center_{1}, single_track_height_{1};
  // Image Mode
  int image_hbin_{1}, image_vbin_{1}, image_hstart_{1},
      image_hend_{MAX_PIXELS_IN_DIM}, image_vstart_{1},
      image_vend_{MAX_PIXELS_IN_DIM};

  /* options for acquisition mode */
  AcquisitionMode acquisition_mode_{AcquisitionMode::RunTillAbort};
  int num_accumulations_{1}; // relevant for Accumulate and KineticSeries
  float accumulation_cycle_time_{
      0.2}; // relevant for Accumulate and KineticSeries; units is seconds
  float kinetics_cycle_time_{0.5}; // relevant for KineticSeries and
                                   // RunTillAbort; units is seconds

  /* shutter options */
  ShutterMode shutter_mode_{ShutterMode::FullyAuto};
  int shutter_closing_time_{50}, // in milliseconds
      shutter_opening_time_{50}; // in milliseconds

  /* cooler mode
    1 – Temperature is maintained on ShutDown
    0 – Returns to ambient temperature on ShutDown
  */
  bool cooler_mode_{0};

  /* number of tries to access aristarchos headers; if > 0 then we will try to
   * access Aristarchos and get/decode its headers. If sucessefull, the headers
   * will be added to the saved FITS image(s). If set to 0, then we will not
   * try to acess Aristarchos
   */
  int ar_hdr_tries_ = 0;

}; // AndorParameters

inline int ReadOutMode2int(ReadOutMode rom) noexcept {
  return static_cast<int>(rom);
}

inline int AcquisitionMode2int(AcquisitionMode am) noexcept {
  return static_cast<int>(am);
}

inline int ShutterMode2int(ShutterMode sm) noexcept {
  return static_cast<int>(sm);
}

const char *date_str(char *buf) noexcept;

int setup_read_out_mode(const AndorParameters *) noexcept;

int setup_acquisition_mode(const AndorParameters *) noexcept;

int resolve_cmd_parameters(int argc, char *argv[],
                           AndorParameters &params) noexcept;

int cool_to_temperature(int temp_in_celsius) noexcept;

int select_camera(int camera_index = 0) noexcept;

int system_shutdown() noexcept;

int print_status() noexcept;

int get_next_fits_filename(const AndorParameters *params,
                           char *fits_fn) noexcept;

int setup_acquisition(const AndorParameters *params, FitsHeaders *fheaders,
                      int &width, int &height, float &vsspeed, float &hsspeed,
                      at_32 *&img_mem) noexcept;

int get_acquisition(const AndorParameters *params, FitsHeaders *fheaders,
                    int xnumpixels, int ynumpixels, at_32 *img_buffer, const andor2k::Socket& socket) noexcept;

int set_fastest_recomended_vh_speeds(float &vspeed, float &hspeed) noexcept;

int coarse_exposure_time(const AndorParameters* params, long &millisec_per_image, long &total_millisec) noexcept;

char *get_status_string(char *buf) noexcept;
char *get_start_acquisition_status_string(unsigned int error,
                                          char *buffer) noexcept;
#endif
