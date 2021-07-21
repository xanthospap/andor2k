#include "andor2k.hpp"
#include <cstring>
#include <ctime>
#include <filesystem>
#include <optional>
#include <system_error>

namespace fs = std::filesystem;

// forward decleration
int get_date_string_utc(char *buf) noexcept;
std::optional<fs::path>
make_fits_filename(const AndorParameters *params) noexcept;

/// @brief Formulate the next to-be-saved FITS filename to avoid collisions
/// Basically, we are saving FITS files, using the convention:
/// [GENERIC_FN][YYYYMMDD][INDEX].fits
/// where GENERIC_FN is extracted from the parameters instance (aka the member
/// variable image_filename_).
/// To avoid filename collisions/overwritting, we add an INDEX at the end of the
/// filename, which sould be unique. This function will do exactly that:
/// formulate a FITS filename that is unique in the params.save_dir_.
/// It will first formulate the filename aka: [GENERIC_FN][YYYYMMDD]1.fits
/// It will then search through the save dir and find any files named the same,
/// with an index equal to or grater than 1. It will then replace the index "1"
/// with the respective index (e.g. if we already have a filename
/// [GENERIC_FN][YYYYMMDD]6.fits, it will return the filename
/// [GENERIC_FN][YYYYMMDD]7.fits) and prepend the save dir.
/// Note that the function will formulate the whole filename (including path).
/// @param[out] fits_fn The next-to-be-saved FITS file (including path). The
///            funtion will fill this string with a valid filename. Only use
///            the string if the function returns 0.
/// @return An integer; if other than 0, then the function failed.
int get_next_fits_filename(const AndorParameters *params,
                           char *fits_fn) noexcept {
  auto fits_fn_path = make_fits_filename(params);
  if (fits_fn_path) {
    std::memset(fits_fn, '\0', MAX_FITS_FILE_SIZE);
    std::strcpy(fits_fn, fits_fn_path.value().string().c_str());
  } else {
    char buf[32] = {'\0'}; /* buffer for datetime string */
    fprintf(stderr,
            "[ERROR][%s] Failed to formulate a valid FITS filename (traceback: "
            "%s)\n",
            date_str(buf), __func__);
    return 1;
  }
  return 0;
}

/// @brief Formulate the next to-be-saved FITS filename to avoid collisions
/// Basically, we are saving FITS files, using the convention:
/// [GENERIC_FN][YYYYMMDD][INDEX].fits
/// where GENERIC_FN is extracted from the parameters instance (aka the member
/// variable image_filename_).
/// To avoid filename collisions/overwritting, we add an INDEX at the end of the
/// filename, which sould be unique. This function will do exactly that:
/// formulate a FITS filename that is unique in the params.save_dir_.
/// It will first formulate the filename aka: [GENERIC_FN][YYYYMMDD]1.fits
/// It will then search through the save dir and find any files named the same,
/// with an index equal to or grater than 1. It will then replace the index "1"
/// with the respective index (e.g. if we already have a filename
/// [GENERIC_FN][YYYYMMDD]6.fits, it will return the filename
/// [GENERIC_FN][YYYYMMDD]7.fits) and prepend the save dir.
/// Note that the function will return the whole filename (including path), or
/// nothing in case something goes wrong.
/// @return An optional fs::path; on success, it will hold the next-to-be-saved
///         FITS filename (including path)
std::optional<fs::path>
make_fits_filename(const AndorParameters *params) noexcept {

  char buf[32] = {'\0'}; /* buffer for datetime string */
  char filename[MAX_FITS_FILENAME_SIZE] = {'\0'}; /* filename (including path */
  std::error_code ec;

  /* first of all, check that the save path exists */
  fs::path sdir(params->save_dir_);
  if (!fs::is_directory(sdir, ec)) {
    fprintf(
        stderr,
        "[ERROR][%s] Path \"%s\" is not a valid directory! (traceback: %s)\n",
        date_str(buf), params->save_dir_, __func__);
    return {};
  }

  /* format the generic file name */
  std::strcpy(filename, params->image_filename_);
  std::size_t sz = std::strlen(filename);
  if (get_date_string_utc(filename + sz)) {
    fprintf(stderr,
            "[ERROR][%s] Failed to get current datetime! (traceback: %s)\n",
            date_str(buf), __func__);
    return {};
  }
  sz = std::strlen(filename);

  /* loop through all files in folder ... */
  const char *existing_fn;
  char *end;
  int img_count = 1, current_count = 1;
  for (auto const &entry : fs::directory_iterator(params->save_dir_)) {
    /* get files's filename.
    have to use an intermidiate string...else shit happens; dont know why */
    std::string fn_str = entry.path().filename().string();
    existing_fn = fn_str.c_str();
    /* if generic filename matches */
    if (!std::strncmp(filename, existing_fn, sz)) {
      /* (try to) get index of existing filename */
      current_count = std::strtol(existing_fn + sz, &end, 10);
      /* could we reolve an index ? */
      if (end != existing_fn + sz && *end == '.') {
        /* set the image counter accordingly */
        if (current_count >= img_count) {
          img_count = current_count + 1;
          /*printf(
              "[DEBUG][%s] Save directory already contains an exposure of this "
              "set, aka \"%s\";\n",
              date_str(buf), existing_fn);
          printf("[DEBUG][%s] Setting counter to %3d\n", date_str(buf),
                 img_count);*/
        } /* current image counter already larger that this file's index */
      }   /* failed to resolve index; do not care about this file */
    }     /* filename and file in folder do not match */
  }       /* end looping through files */

  /* add image counter and extension to filename */
  std::sprintf(filename + sz, "%d", img_count);
  std::strcat(filename, ".fits");

  /* prepend save directory */
  return sdir /= filename;
}

/// @brief Write current date string (YYYYMMDD) to buf
/// @param[in] buf Char array to write date string to
/// @return Anything other than 0 denotes an error
int get_date_string_utc(char *buf) noexcept {
  std::time_t now = std::time(nullptr);
  std::tm *now_utc = ::gmtime(&now);
  if (!now_utc) {
    fprintf(stderr,
            "[ERROR][%s] Failed to get current datetime! (traceback: %s)\n",
            date_str(buf), __func__);
    return 1;
  }
  // should write 8 chars (4+2+2) not including the null terminating char
  std::strftime(buf, MAX_FITS_FILENAME_SIZE, "%Y%m%d", now_utc);
  return 0;
}
