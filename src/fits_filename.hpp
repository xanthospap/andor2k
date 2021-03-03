#ifndef __ARISTARCHOS_FITS_FN_HPP__
#define __ARISTARCHOS_FITS_FN_HPP__

#include <string>

namespace aristarchos {

namespace constants {
const char image_dir[] = "/data/rise2";
const char instrument_code_ccd_camera = 'q';
const char exposure_code_exposure = 'e';
const char exposure_code_bias = 'b';
const char exposure_code_standard = 's';
const char exposure_code_flat = 'f';
const char exposure_code_lamp_flat = 'w';
const char exposure_code_arc = 'a';
const char exposure_code_dark = 'd';
const int pipeline_processing_flag_none = 0;
const int pipeline_processing_flag_real_time = 1;
const int pipeline_processing_flag_off_line = 2;
/// @brief maximum number of characters in a FITS filename (excluding path)
constexpr int FITS_FN_MAX_CHARS = 64;
} // namespace constants

/// @brief A class to hold a FitsFilename as a collection of fields. Note that
///        the naming convention of such files is the following:
///        '.{1}_.{1}_.{8}_[0-9]+_[0-9]+_[0-9]+_[0-9]+(.*)'
///           |    |    |    |      |      |      |
///       instCode |    date |   runNumber |  plProcessing
///           exposureType multRunNumber windowNumber
///        Example filename : 'c_e_20070830_11_10_1_0.fits'
struct FitsFilename {
  const char *directory{constants::image_dir};
  char date[16]{"19700101"}; ///< stores (C-style) date string as "%Y%m%d"
  char file_ext[8]{"fits"};
  char instrument_code{constants::instrument_code_ccd_camera};
  char exposure_code{constants::exposure_code_exposure};
  int pipeline_processing{constants::pipeline_processing_flag_none};
  int multi_run_nr{0};
  int run_nr{0};
  int window_number{1};
  int is_tel_focus{false};
  int is_twilight_calibrate{false};

  int decompose(const std::string &fn) noexcept;

  int as_str(char *fnstr) noexcept;

}; // FitsFilename

int get_current_date_str(char *buf) noexcept;
FitsFilename find_max_FitsFilename(const char *source_dir,
                                   const std::string &substr,
                                   int &status) noexcept;
FitsFilename get_next_FitsFilename(bool start_new_multirun, int exposure_int,
                                   int &status,
                                   const char *src_dir = nullptr) noexcept;
} // namespace aristarchos

#endif
