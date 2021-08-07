#include "andor2k.hpp"
#include "andor_time_utils.hpp"
#include "atmcdLXd.h"
#include <chrono>
#include <cstdio>
#include <cstring>

using namespace std::chrono;

/// This function corrects the multrun epoch time so that the time of the
/// start of image acquisition is taken. It subtracts the readout time and
/// the frame transfer time, derived from the horizontal and vertical
/// shift speeds. These speeds are in microseconds per pixel. Note tv_nsec
/// is in nanoseconds so that 1 microsec = 1000 nanosec!
///
/// For a single Multrun, the correction will be the same for each image, as it
/// is a function of VSspeed, HSspeed and the exposure time. Use in
/// conjunction with correct_start_time() to get the UTSTART struct
/// timespec.
/// @param[in] exposure Exposure time in seconds (see note 1)
/// @param[in] image_rows Number of rows in image
/// @param[in] image_cols Number of columns in image
/// @param[in] vsspeed vertical shift speed in microseconds per pixel
/// @param[in] hsspeed horizontal shift speed in microseconds per pixel
/// @return Returns the value in nanoseconds of the correction
/// @note
///  - The exposure time should be the value computed by the ANDOOR system, not
///    the one supllied by the user. that is because the ANDOR system can modify
///    the user-supplied exposure time to make it valid (considering other
///    options passed in). See the SDK supplied GetAcquisitionTimings function.
double start_time_correction_impl(float exposure, float vsspeed, float hsspeed,
                             int img_rows, int img_cols) noexcept {
  double dex = static_cast<double>(exposure);
  double vsp = static_cast<double>(vsspeed);
  double hsp = static_cast<double>(hsspeed);

  /* Get the readout time in microseconds */
  double readout_time = (img_rows * vsp) + (img_cols * img_rows * hsp);
  double ft_time = img_rows * vsp;

#ifdef DEBUG
  char buf[32];
  printf("[DEBUG][%s] Computed start_time_correction to be: %15.2f + %15.2f = "
         "%15.2f nanoseconds\n",
         date_str(buf), ft_time, img_cols * img_rows * hsp,
         readout_time);
  printf("[DEBUG][%s] Total start_time_correction is: %15.2f nanoseconds\n",
         date_str(buf), readout_time * 1e3 + ft_time * 1e3 + exposure * 1e9);
  float andor_rot;
  if (GetReadOutTime(&andor_rot) != DRV_SUCCESS) {
    printf("[DEBUG][%s] Failed to the readout time from ANDOR2K system; can\'t "
           "compare with computed value (traceback: %s)\n",
           date_str(buf), __func__);
  } else {
    printf("[DEBUG][%s] ANDOR2K gives a readouttime of %15.2f nanoseconds\n",
           date_str(buf), andor_rot * 1e9);
  }
#endif
  
  /* Return time correction in nanoseconds */
  return (readout_time * 1e3 + ft_time * 1e3 + dex * 1e9);
}

std::chrono::nanoseconds start_time_correction(float exposure, float vsspeed, float hsspeed,
                             int img_rows, int img_cols) noexcept {
double corr_ns = start_time_correction_impl(exposure, vsspeed, hsspeed, img_rows, img_cols);
long lcorr_ns = std::round(corr_ns);
return std::chrono::nanoseconds(lcorr_ns);
}

std::tm strfdt_work(const std_time_point& t, long& fractional_seconds) noexcept {
  std::chrono::milliseconds ms = std::chrono::duration_cast<std::chrono::milliseconds>(t.time_since_epoch());
  // std::chrono::seconds s = std::chrono::duration_cast<std::chrono::seconds>(ms);
  std::time_t tt = std::chrono::system_clock::to_time_t(t);
  std::tm tm = *std::gmtime(&tt); //GMT (UTC)
  fractional_seconds = ms.count() % 1000;
  return tm;
}