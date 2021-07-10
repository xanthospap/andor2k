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

int get_next_fits_filename(const AndorParameters *params,
                           char *fits_fn) noexcept {
  auto fits_fn_path = make_fits_filename(params);
  if (fits_fn_path) {
    std::strcpy(fits_fn, fits_fn_path.value().string().c_str());
  } else {
    return 1;
  }
  return 0;
}

std::optional<fs::path>
make_fits_filename(const AndorParameters *params) noexcept {

  char filename[max_fits_filename_size] = {'\0'};
  std::error_code ec;

  // first of all, check that the save path exists
  fs::path sdir(params->save_dir_);
  if (!fs::is_directory(sdir, ec)) {
    fprintf(stderr, "[ERROR] Path \"%s\" is not a valid directory!\n",
            params->save_dir_);
    return {};
  }

  // format the generic file name
  std::strcpy(filename, params->type_);
  std::size_t sz = std::strlen(filename);

  if (get_date_string_utc(filename + sz)) {
    fprintf(stderr, "[ERROR] Failed to get current datetime!\n");
    return {};
  }
  sz += 8;

  // find any filename in the folder, with same generic name
  const char *existing_fn;
  char *end;
  int img_count = 0, current_count = 0;
  for (auto const &entry : fs::directory_iterator(params->save_dir_)) {
    existing_fn = entry.path().filename().string().c_str();
    if (!std::strncmp(filename, existing_fn, sz)) {
      current_count = std::strtol(existing_fn + sz, &end, 10);
      if (end != existing_fn + sz && *end == '.') {
        if (current_count >= img_count) {
          printf("[DEBUG] Save directory already contains an exposure of this "
                 "set, aka \"%s\";\n",
                 existing_fn);
          printf("[DEBUG] Setting counter to %3d\n", ++img_count);
        }
      }
    }
  }

  std::sprintf(filename + sz, "%d", img_count);
  std::strcat(filename, ".fits");

  return sdir /= filename;
}

int get_date_string_utc(char *buf) noexcept {
  // get current datetime
  std::time_t now = std::time(nullptr);

  // convert now to Coordinated Universal Time (UTC).
  std::tm *now_utc = ::gmtime(&now);
  if (!now_utc)
    return 1;

  // should write 8 chars (4+2+2) not including the null terminating char
  std::strftime(buf, 9, "%Y%m%d", now_utc);

  return 0;
}