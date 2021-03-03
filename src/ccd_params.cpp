#include "ccd_params.hpp"
#include <cerrno>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <iostream>

/// @brief max line size in a CCD Parameter file.
constexpr int LINE_SIZE = 256;

/// @details A CCD parameter file is just an ascii file that contains parameter
///          values. In this file, we are looking for a line of type:
///          "CCD_TARGET_TEMP SomeIntegerValue"
///          The start of line can contain any number of whitespace characters;
///          they will be discarded before reading. The "CCD_TARGET_TEMP"
///          string and the parameter value should be split by (any number of)
///          whitespace characters. Aka, all lines below are ok:
///          "CCD_TARGET_TEMP -20"
///          "CCD_TARGET_TEMP                                              -20"
///          "                                CCD_TARGET_TEMP          -20"
///          If the line is found and parsed correctly, the "CCD_TARGET_TEMP"
///          value will be assigned to the instance's m_targetTemp variable.
/// @param[in] filename The filename of the CCD parameters file
/// @return An integer denoting the function status, as follows:
///         Value | Explanation
///           -2  | Failed to resolve CCD_TARGET_TEMP value
///           -1  | Parameter file could not be opened
///            0  | CCD_TARGET_TEMP value parsed and assigned to instance
///            1  | CCD_TARGET_TEMP not found in file
/// @warnng No line in file should be more than LINE_SIZE chars long.
int aristarchos::CCDParams::read_CCD_params(const char *fn) noexcept {
  std::ifstream fin{fn, std::ios::in};
  if (!fin.is_open()) {
    std::cerr << "\n[ERROR] Failed opening CCD Parameter file " << fn << "\n";
    return -1;
  }

  char line[LINE_SIZE];
#if defined(__clang__)
  const int CCD_TARGET_TEMP_SZ = std::strlen("CCD_TARGET_TEMP");
#else
  constexpr int CCD_TARGET_TEMP_SZ = std::strlen("CCD_TARGET_TEMP");
#endif
  char *p_end;
  int status = 1;
  while (fin.getline(line, LINE_SIZE)) {
    // skip any whitespace characters
    int start_idx = 0;
    while (*(line + start_idx) && *(line + start_idx) == ' ')
      ++start_idx;
    if (!std::strncmp(line + start_idx, "CCD_TARGET_TEMP",
                      CCD_TARGET_TEMP_SZ)) {
      int val = std::strtol(line + CCD_TARGET_TEMP_SZ, &p_end, 10);
      if ((line + CCD_TARGET_TEMP_SZ == p_end) || errno) {
        std::cerr << "\n[ERROR] Failed extracting \"CCD_TARGET_TEMP\" value "
                     "from line: \""
                  << line << "\"";
        errno = 0;
        return -2;
      }
      this->m_target_temp = val;
      status = 0;
    }
  }

  return status;
}
