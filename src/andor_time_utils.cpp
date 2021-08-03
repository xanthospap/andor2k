#include "andor2k.hpp"
#include "atmcdLXd.h"
#include <cstdio>
#include <cstring>
#include "date/date.h"
#include <chrono>

using namespace date;
using namespace std::chrono;

#define CCD_GLOBAL_ONE_MILLISECOND_NS	(1000000)
#include <ctime>

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
double start_time_correction(float exposure, int image_rows, int image_cols,
                             float vsspeed, float hsspeed) noexcept {
  /* Get the readout time in microseconds */
  double ft_time = image_rows * vsspeed;
  double readout_time = ft_time + (image_cols * image_rows * hsspeed);

#ifdef DEBUG
  char buf[32];
  printf("[DEBUG][%s] Computed start_time_correction to be: %15.2f + %15.2f = "
         "%15.2f nanoseconds\n",
         date_str(buf), ft_time, imageCols * image_rows * hsspeed,
         readout_time);
  printf("[DEBUG][%s] Total start_time_correction is:  nanoseconds\n",
         date_str(buf), readout_time * 1e3 + ft_time * 1e3 + exposure * 1e9);
  float andor_rot;
  if (GetReadOutTime(andor_rot) != DRV_SUCCESS) {
    printf("[DEBUG][%s] Failed to the readout time from ANDOR2K system; can\'t "
           "compare with computed value (traceback: %s)\n",
           date_str(buf), __func__);
  } else {
    printf("[DEBUG][%s] ANDOR2K gives a readouttime of %15.2f nanoseconds\n",
           date_str(buf), andor_rot * 1e9);
  }
#endif

  /* Return time correction in nanoseconds */
  return (readout_time * 1e3 + ft_time * 1e3 + exposure * 1e9);
}



void Exposure_TimeSpec_To_Date_Obs_String(struct timespec time,char *time_string)
{
	struct tm *tm_time = NULL;
	char buff[32];
	long milliseconds;
	
	tm_time = gmtime(&(time.tv_sec));
	strftime(buff,32,"%Y-%m-%dT%H:%M:%S.",tm_time);
	milliseconds = (long)(((double)time.tv_nsec)/((double)CCD_GLOBAL_ONE_MILLISECOND_NS));
	sprintf(time_string,"%s%03ld",buff,milliseconds);
}


void Exposure_TimeSpec_To_UtStart_String(struct timespec time,char *time_string)
{
	struct tm *tm_time = NULL;
	char buff[16];
	long milliseconds;
	
	tm_time = gmtime(&(time.tv_sec));
	strftime(buff,16,"%H:%M:%S.",tm_time);
	milliseconds = (long)(((double)time.tv_nsec)/((double)CCD_GLOBAL_ONE_MILLISECOND_NS));
	sprintf(time_string,"%s%03ld",buff,milliseconds);
}

/**
 * Routine to convert a timespec structure to a DATE sytle string to put into a FITS header.
 * This uses gmtime and strftime to format the string. The resultant string is of the form:
 * <b>CCYY-MM-DD</b>, which is equivalent to %Y-%m-%d passed to strftime.
 * @param time The time to convert.
 * @param time_string The string to put the time representation in. The string must be at least
 * 	12 characters long.
 */
void Exposure_TimeSpec_To_Date_String(struct timespec time,char *time_string)
{
	struct tm *tm_time = NULL;
	
	tm_time = gmtime(&(time.tv_sec));
	strftime(time_string,12,"%Y-%m-%d",tm_time);
}