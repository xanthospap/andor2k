#include "fits_filename.hpp"
#include <cassert>
#include <cerrno>
#include <cstdio>
#include <cstring>
#include <ctime>
#include <filesystem>

/// @brief Construct a string representing this instance based on its members.
/// The filename constructed follows the naming convention:
/// '.{1}_.{1}_.{8}_[0-9]+_[0-9]+_[0-9]+_[0-9]+(.*)', for example:
/// 'c_e_20070830_11_10_1_0.fits'.
/// @param[in] fnstr A char buffer to store the constructed filename. Must
///                  have enough size. To be sure, allocate a buffer of size
///                  constants::FITS_FN_MAX_CHARS.
/// @return The return status of std::sprintf. That is, The number of bytes
///         written into the character array pointed to by fnstr not including
///         the terminating '\0' on success. If count was reached before the
///         entire string could be stored, 0 returned and the contents are
///         undefined.
/// @todo how many digits in pipeline_processing (when printing)?
int aristarchos::FitsFilename::as_str(char *fnstr) noexcept {
  return std::sprintf(fnstr, "%c_%c_%s_%04d_%04d_%d_%02d.%s", instrument_code,
                      exposure_code, date, multi_run_nr, run_nr, window_number,
                      pipeline_processing, file_ext);
}

/// @brief Return current UTC date as a null-terminating string as '%Y%m%d'.
/// The function will get the current datetime, convert it to UTC and then
/// print it out to the given char buffer in the format '%Y%m%d' (aka 8 chars
/// plus the null terminating character).
/// @param[out] buf A character buffer (array) of lebgth at least 9
/// @return An integer denoting the function status, as follows:
///         Returned Value | Status
///         ---------------|---------------------------------
///                    < 0 | failed to convert current datetime to UTC
///                      0 | success
///                    > 0 | failed to write date to buffer
///
/// @warning The date string needs to be the date AT THE START OF THE NIGHT.
///          If the hour of day is less than midday, then the date at the
///          start of the night is the day before the current day so we roll
///          back the date by one day.
int aristarchos::get_current_date_str(char *buf) noexcept {
  // get current datetime
  std::time_t now = std::time(nullptr);
  // convert now to Coordinated Universal Time (UTC).
  std::tm *now_utc = ::gmtime(&now);
  if (!now_utc)
    return -1;
  // The date string needs to be the date AT THE START OF THE NIGHT.
  // If the hour of day is less than midday, then the date at the start of the
  // night is the day before the current day so roll back the date by one day.
  if (now_utc->tm_hour < 12) {
    now = now - (std::time_t)(24 * 60 * 60);
    if (!(now_utc = ::gmtime(&now)))
      return -2;
  }
  // should write 8 chars (4+2+2) not including the null terminating char
  return !(std::strftime(buf, 9, "%Y%m%d", now_utc) == 8);
}

/// @brief Parse a fits filename string (excluding path) to a FitsFilename
///        instance.
/// The filename must have a naming convention:
/// '.{1}_.{1}_.{8}_[0-9]+_[0-9]+_[0-9]+_[0-9]+(.*)', for example:
/// 'c_e_20070830_11_10_1_0.fits'. If any of the fields is missing or a
/// character-to-int  conversion fails, the function will fail (returning
/// an integer othher than 0).
/// On success, the relevant members of the instance will be assigned the
/// values parsed off from the filename string.
/// @param[in]  fn  The filename to parse
/// @return An integer denoting the function status. Anything other than 0
///         denotes an error
///         Return Value | Status
///         -------------|---------------------------------------
///                  < 0 | propably invalid filename
///                    0 | success
///                  > 0 | parsing error
/// @note The '.fits' part of the filename (if it exists) is not checked and
///       can be missing (or can be any character sequence).
/// TODO check that the date field is parsed as ints-only
///      chekt that pipeline processing nr is [0-2]
///      parse filename extension
int aristarchos::FitsFilename::decompose(const std::string &fn) noexcept {
  if (fn.size() + 1 > constants::FITS_FN_MAX_CHARS)
    return -1;
  // copy filename to temporary buffer cause std::strtok will modify it
  char fnc[64];
  if (!std::strcpy(fnc, fn.c_str()))
    return -2;
  // variables for parsing
  char *token, *p_end;
  char delim[] = "_";
  int ints[4];
  // iteratively tokenize the string and parse fields
  // example filename: 'c_e_20070830_11_10_1_0.fits'
  this->instrument_code = (token = std::strtok(fnc, delim)) ? *token : 'x';
  if (!token || std::strlen(token) != 1)
    return 1;
  this->exposure_code = (token = std::strtok(nullptr, delim)) ? *token : 'x';
  if (!token || std::strlen(token) != 1)
    return 2;
  (token = std::strtok(nullptr, delim)) ? std::strncpy(this->date, token, 8)
                                        : std::strncpy(this->date, "x", 2);
  if (!token || std::strlen(token) != 8)
    return 3;
  for (int i = 0; i < 4; i++) {
    ints[i] = (token = std::strtok(nullptr, delim))
                  ? std::strtol(token, &p_end, 10)
                  : -999;
    if (!token || (p_end == token || errno)) {
      errno = 0;
      return 3 + i;
    }
  }
  this->multi_run_nr = ints[0];
  this->run_nr = ints[1];
  this->window_number = ints[2];
  this->pipeline_processing = ints[3];

  return 0;
}

/// @brief Returns the file in directory sourceDir which has the largest
///        multiRun & run number.
/// The function will iterate all files in the directory source_dir (including
/// links) excluding any other sub-directories. I.e. subfolders in sourceDir
/// will not be considered.
/// For every file, if the substr string is contained in the files's filename
/// (excluding path), then this filename will be checked, else it will be
/// ignored.
/// For every file to be checked, decompose it to an FitsFilename instance.
/// Find the FitsFilename instance with the highest multi_run_nr & run_nr
/// combination. Return the max FitsFilename instance.
/// If the folder does not exist or is empty, or no file in the folder could
/// be matched and/or parsed, a default-constructed FitsFilename instance will
/// be returned and the status variable will be set accordingly. Else, status
/// will be set to 0.
/// Remember that for a filename to be parsed as a FitsFilename isntance, it
/// needs to conform to the naming convention:
/// '.{1}_.{1}_.{8}_[0-9]+_[0-9]+_[0-9]+_[0-9]+(.*)',
/// e.g. 'c_e_20070830_11_10_1_0.fits' (see FitsFilename::decompose).
/// @param[in] sourceDir The directory to search at; the last character can be
///                      the '/' char or not, i.e. "/bar/baz/foo/" is the same
///                      as "/bar/baz/foo".
/// @param[in] substr    The string to match against the files in the sourceDir
///                      directory. Any file containing this string will be
///                      parsed and checked.
/// @param[out] status   An integer denoting the function status; that is:
///                      status < 0 : source_dir does not exist.
///                      status > 0 : source_dir exists but no file found that
///                                   contains the substr or that could be
///                                   succesefuly decomposed to a FitsFilename.
///                      status = 0 : At least one matching file found and
///                                   parsed to FitsFilename.
/// @return A vector of filesystem::path, aka the files in the sourceDir for
///        which the filename contains the substr string.
/// @note The returned instance's directory pointer will point to the
///       source_dir char array provided at input. Beware of what you do with
///       it.
aristarchos::FitsFilename aristarchos::find_max_FitsFilename(
    const char *source_dir, const std::string &substr, int &status) noexcept {
  namespace fs = std::filesystem;
  int max_multi_run = 0, max_run = 0;
  FitsFilename maxfn, tmpfn;
  maxfn.directory = tmpfn.directory = source_dir;
  if (fs::exists(source_dir) && fs::is_directory(source_dir)) {
    status = 1; // set status to directory_exists
    // for every file in the source directory ...
    for (auto const &entry : fs::directory_iterator(source_dir)) {
      auto fn_str = entry.path().filename().string();
      // if substr is a substring of the filename ...
      if (fn_str.find(substr) != std::string::npos) {
        // decompose filename to a struct instance ...
        if (!tmpfn.decompose(fn_str)) {
          status = 0; // at least one file found and decomposed
          // if multi_run_number > max, this is the max file
          if (tmpfn.multi_run_nr > max_multi_run) {
            maxfn = tmpfn;
            max_multi_run = tmpfn.multi_run_nr;
            max_run = tmpfn.run_nr;
            // if multi_run_number is max, and run_number > max, this is max
            // file
          } else if (tmpfn.multi_run_nr == max_multi_run &&
                     tmpfn.run_nr > max_run) {
            maxfn = tmpfn;
            max_run = tmpfn.run_nr;
          }
        }
      }
    }
  } else {
    // directory does not exist
    status = -1;
  }
  return maxfn;
}

/// @brief Construct the next FitsFilename to be created after inspecting all
///        relevant files in the constants::image_dir directory.
/// The function will:
/// 1. Search through the constants::image_dir directory and find all
///    FitsFilename conformant files belonging to the current date. Out of
///    these, identify the max FitsFilename depending on their 'multi_run' and
///    'run' identifiers. If the directory does not exist, status is set to
///    -1. See findMaxFitsFilename.
/// 2. Construct and return the next-to-be-created FitsFilename; that is if a
///    max FitsFilename is found in the previous step, copy it and change the
///    multi_run_nr and run_nr member variables to reflect the next file. If no
///    file was found in step (1), the create a default-constructed one, and
///    set the multi_run_nr, run_nr and date member variables accordingly. In
///    any case, the returned FitsFilename will have an exposure_code defined
///    by exposure_int.
/// The status variable is set from the findMaxFitsFilename function call;
/// that is, status will be set to -1 if the directory does not exist, >0 if
/// no file was matched and/or parsed and 0 in case at least one file was
/// matched and successefully parsed.
/// @param[in] start_new_multirun Whether or not to start a new MultiRun
/// @param[in] exposure_int The exposure code to be set to the returned
///            FitsFilename.
/// @param[out] status Denotes the function status, as follows:
///             status will be set to -1 if the directory does not exist,
///             status will be set to >0 if no file was matched and/or parsed
///             0 at least one file was matched and successefully parsed
/// @param[in] src_dir Optionally privide a source directory where the function
///            will search for the Fits files. If ommited, constants::image_dir
///            will be considered as the source dir. See note below.
/// @return The next in que FitsFilename to be constructed.
/// @note The returned instance's directory pointer will point to the
///       source_dir char array provided at input. Beware of what you do with
///       it.
aristarchos::FitsFilename
aristarchos::get_next_FitsFilename(bool start_new_multirun, int exposure_int,
                                   int &status, const char *src_dir) noexcept {
  const char *source_dir = src_dir ? src_dir : constants::image_dir;
  // get the max FitsFilename of all filenames that contain the current date
  char today_str[16];
  assert(!get_current_date_str(&today_str[0]));
  FitsFilename maxfn =
      find_max_FitsFilename(source_dir, std::string(today_str), status);
  // if no max file found matched, maxfn is default-constructed; could also be
  // that the directory was not found
  if (status) {
    std::strcpy(maxfn.date, today_str);
    maxfn.multi_run_nr = 0;
    maxfn.run_nr = 0;
  }
  // make the next FitsFilename depending on if we want a new multirun
  if (!start_new_multirun) {
    ++(maxfn.run_nr);
  } else {
    ++(maxfn.multi_run_nr);
    maxfn.run_nr = 1;
  }
  // wait!!! we must set the exposure_code
  if (exposure_int == 1)
    maxfn.exposure_code = constants::exposure_code_flat;
  else if (exposure_int == 2)
    maxfn.exposure_code = constants::exposure_code_bias;
  else
    maxfn.exposure_code = constants::exposure_code_exposure;
  return maxfn;
}
